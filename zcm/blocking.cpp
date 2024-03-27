#include "zcm/zcm.h"
#include "zcm/zcm_private.h"
#include "zcm/blocking.h"
#include "zcm/transport.h"
#include "zcm/zcm_coretypes.h"
#include "zcm/util/topology.hpp"

#include "util/TimeUtil.hpp"
#include "util/debug.h"

#include <cassert>
#include <cstring>
#include <utility>

#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <regex>
#include <atomic>
#include <condition_variable>
using namespace std;

#define RECV_TIMEOUT 100

// Define a macro to set thread names. The function call is
// different for some operating systems
#ifdef __linux__
    #define SET_THREAD_NAME(name) pthread_setname_np(pthread_self(),name)
#elif __FreeBSD__ || __OpenBSD__
    #include <pthread_np.h>
    #define SET_THREAD_NAME(name) pthread_set_name_np(pthread_self(),name)
#elif __APPLE__ || __MACH__
    #define SET_THREAD_NAME(name) pthread_setname_np(name)
#else
    // If the OS is not in this list, don't call anything
    #define SET_THREAD_NAME(name)
#endif

static bool isRegexChannel(const string& channel)
{
    // These chars are considered regex
    auto isRegexChar = [](char c) {
        return c == '(' || c == ')' || c == '|' ||
        c == '.' || c == '*' || c == '+';
    };

    for (auto& c : channel)
        if (isRegexChar(c))
            return true;

    return false;
}

struct zcm_blocking
{
  private:
    // XXX If we change this to a linked list implementation, we can probably
    //     support subscribing/unsubscribing from within subscription callback handlers
    using SubList = vector<zcm_sub_t*>;

  public:
    zcm_blocking(zcm_t* z, zcm_trans_t* zt_);
    ~zcm_blocking();

    void run();
    void start();
    void stop();
    void pause();
    void resume();
    int handle(unsigned timeout);
    int flush();
    int setQueueSize(unsigned numMsgs);

    int publish(const char* channel, const uint8_t* data, uint32_t len);
    zcm_sub_t* subscribe(const string& channel, zcm_msg_handler_t cb, void* usr, bool block);
    int unsubscribe(zcm_sub_t* sub, bool block);

    int getNumDroppedMessages();
    int writeTopology(string name);

  private:
    void recvThreadFunc();
    bool startRecvThread();

    void dispatchMsg(const zcm_msg_t& msg);
    int recvOneMessage(unsigned timeout);

    bool deleteSubEntry(zcm_sub_t* sub, size_t nentriesleft);
    bool deleteFromSubList(SubList& slist, zcm_sub_t* sub);

    zcm_t* z;
    zcm_trans_t* zt;
    unordered_map<string, SubList> subs;
    unordered_map<string, SubList> subsRegex;
    size_t mtu;

    zcm::TopologyMap receivedTopologyMap;
    zcm::TopologyMap sentTopologyMap;

    // These 2 mutexes used to implement a read-write style infrastructure on the subscription
    // lists. Both the recvThread and the message dispatch may read the subscriptions
    // concurrently, but subscribe() and unsubscribe() need both locks in order to write to it.
    mutex subMutex;

    thread recvThread;

    enum class RunState {NONE, RUNNING, PAUSE, PAUSED, STOP};
    atomic<RunState> runState {RunState::NONE};
    condition_variable pauseCond;
    mutex pauseCondLk;
};

zcm_blocking_t::zcm_blocking(zcm_t* z_, zcm_trans_t* zt_)
{
    ZCM_ASSERT(z_->type == ZCM_BLOCKING);
    z = z_;
    zt = zt_;
    mtu = zcm_trans_get_mtu(zt);
}

zcm_blocking_t::~zcm_blocking()
{
    // Shutdown all threads
    if (runState != RunState::NONE) stop();

    // Destroy the transport
    zcm_trans_destroy(zt);

    // Need to delete all subs
    for (auto& it : subs) {
        for (auto& sub : it.second) {
            delete sub;
        }
    }
    for (auto& it : subsRegex) {
        for (auto& sub : it.second) {
            delete (regex*) sub->regexobj;
            delete sub;
        }
    }
}

void zcm_blocking_t::run()
{
    if (runState != RunState::NONE) {
        ZCM_DEBUG("Err: call to run() when 'runState != RunState::NONE'");
        return;
    }
    runState = RunState::RUNNING;

    recvThreadFunc();
}

void zcm_blocking_t::start()
{
    if (runState != RunState::NONE) {
        ZCM_DEBUG("Err: call to start() when 'runState != RunState::NONE'");
        return;
    }
    runState = RunState::RUNNING;

    recvThread = thread{&zcm_blocking::recvThreadFunc, this};
}

void zcm_blocking_t::stop()
{
    if (runState != RunState::RUNNING && runState != RunState::PAUSED) return;
    runState = RunState::STOP;
    pauseCond.notify_all();
    if (recvThread.joinable()) recvThread.join();
}

void zcm_blocking_t::pause()
{
    if (runState != RunState::RUNNING) return;
    runState = RunState::PAUSE;
    pauseCond.notify_all();
    unique_lock<mutex> lk(pauseCondLk);
    pauseCond.wait(lk, [this](){ return runState == RunState::PAUSED; });
}

void zcm_blocking_t::resume()
{
    if (runState != RunState::PAUSED) return;
    runState = RunState::RUNNING;
    pauseCond.notify_all();
}

int zcm_blocking_t::handle(unsigned timeoutMs)
{
    if (runState != RunState::NONE && runState != RunState::PAUSED) return ZCM_EINVALID;
    return recvOneMessage(timeoutMs);
}

int zcm_blocking_t::setQueueSize(unsigned numMsgs)
{
    if (runState != RunState::NONE && runState != RunState::PAUSED) return ZCM_EINVALID;
    return zcm_trans_set_queue_size(zt, numMsgs);
}

int zcm_blocking_t::publish(const char* channel, const uint8_t* data, uint32_t len)
{
    // Check the validity of the request
    if (len > mtu) return ZCM_EINVALID;
    if (strlen(channel) > ZCM_CHANNEL_MAXLEN) return ZCM_EINVALID;

    zcm_msg_t msg = {
        .utime = TimeUtil::utime(),
        .channel = channel,
        .len = len,
        // cast await const ok as sendmsg guaranteed to not modify data
        .buf = (uint8_t*)data,
    };
    int ret = zcm_trans_sendmsg(zt, msg);
    if (ret != ZCM_EOK) ZCM_DEBUG("zcm_trans_sendmsg() returned error, dropping the msg!");

#ifdef TRACK_TRAFFIC_TOPOLOGY
    int64_t hashBE = 0, hashLE = 0;
    if (__int64_t_decode_array(data, 0, len, &hashBE, 1) == 8 &&
        __int64_t_decode_little_endian_array(data, 0, len, &hashLE, 1) == 8) {
        sentTopologyMap[channel].emplace(hashBE, hashLE);
    }
#endif

    return ret;
}

// Note: We use a lock on subscribe() to make sure it can be
// called concurrently. Without the lock, there is a race
// on modifying and reading the 'subs' and 'subsRegex' containers
zcm_sub_t* zcm_blocking_t::subscribe(const string& channel,
                                     zcm_msg_handler_t cb, void* usr,
                                     bool block)
{
    unique_lock<mutex> lk(subMutex, std::defer_lock);
    if (block) {
        // Intentionally locking in this order
        lk.lock();
    } else if (!lk.try_lock()) {
        return nullptr;
    }
    int rc;

    rc = zcm_trans_recvmsg_enable(zt, channel.c_str(), true);

    if (rc != ZCM_EOK) {
        ZCM_DEBUG("zcm_trans_recvmsg_enable() didn't return ZCM_EOK: %d", rc);
        return nullptr;
    }

    zcm_sub_t* sub = new zcm_sub_t();
    ZCM_ASSERT(sub);
    strncpy(sub->channel, channel.c_str(), ZCM_CHANNEL_MAXLEN);
    sub->channel[ZCM_CHANNEL_MAXLEN] = '\0';
    sub->callback = cb;
    sub->usr = usr;
    sub->regex = isRegexChannel(channel);
    if (sub->regex) {
        sub->regexobj = (void*) new std::regex(sub->channel);
        ZCM_ASSERT(sub->regexobj);
        subsRegex[channel].push_back(sub);
    } else {
        sub->regexobj = nullptr;
        subs[channel].push_back(sub);
    }

    return sub;
}

// Note: We use a lock on unsubscribe() to make sure it can be
// called concurrently. Without the lock, there is a race
// on modifying and reading the 'subs' and 'subsRegex' containers
int zcm_blocking_t::unsubscribe(zcm_sub_t* sub, bool block)
{
    unique_lock<mutex> lk(subMutex, std::defer_lock);
    if (block) {
        lk.lock();
    } else if (!lk.try_lock()) {
        return ZCM_EAGAIN;
    }

    auto& subsSelected = sub->regex ? subsRegex : subs;

    auto it = subsSelected.find(sub->channel);
    if (it == subsSelected.end()) {
        ZCM_DEBUG("failed to find the subscription channel in unsubscribe()");
        return ZCM_EINVALID;
    }

    bool success = deleteFromSubList(it->second, sub);
    if (!success) {
        ZCM_DEBUG("failed to find the subscription entry in unsubscribe()");
        return ZCM_EINVALID;
    }

    return ZCM_EOK;
}

int zcm_blocking_t::flush()
{
    if (runState != RunState::NONE && runState != RunState::PAUSED) {
        ZCM_DEBUG("Err: must be paused or stopped in order to flush");
        return ZCM_EINVALID;
    }

    int ret;
    do {
        ret = recvOneMessage(0);
        if (ret == ZCM_EAGAIN) ret = ZCM_EOK;
        else if (ret == ZCM_EOK) ret = ZCM_EAGAIN;
    } while (ret == ZCM_EAGAIN);

    return ret;
}

int zcm_blocking_t::getNumDroppedMessages()
{
    return zcm_trans_get_num_dropped_messages(zt);
}

void zcm_blocking_t::recvThreadFunc()
{
    // Intentionally not setting runState = RUNNING because it could conflict
    // with a start -> pause call order
    SET_THREAD_NAME("ZeroCM_receiver");

    while (true) {

        if (runState == RunState::PAUSE) {
            runState = RunState::PAUSED;
            pauseCond.notify_all();
            unique_lock<mutex> lk(pauseCondLk);
            pauseCond.wait(lk, [this](){ return runState != RunState::PAUSED; });
        }

        if (runState == RunState::STOP) break;

        recvOneMessage(RECV_TIMEOUT);
    }

    runState = RunState::NONE;
}

int zcm_blocking_t::recvOneMessage(unsigned timeout)
{
    zcm_msg_t msg;
    int rc = zcm_trans_recvmsg(zt, &msg, timeout);
    if (rc != ZCM_EOK) return rc;
    dispatchMsg(msg);
    return ZCM_EOK;
}

void zcm_blocking_t::dispatchMsg(const zcm_msg_t& msg)
{
    zcm_recv_buf_t rbuf;
    rbuf.recv_utime = msg.utime;
    rbuf.zcm = z;
    rbuf.data = msg.buf;
    rbuf.data_size = msg.len;

    // Note: We use a lock on dispatch to ensure there is not
    // a race on modifying and reading the 'subs' container.
    // This means users cannot call zcm_subscribe or
    // zcm_unsubscribe from a callback without deadlocking.
    bool wasDispatched = false;
    {
        unique_lock<mutex> lk(subMutex);

        // dispatch to a non regex channel
        auto it = subs.find(msg.channel);
        if (it != subs.end()) {
            for (zcm_sub_t* sub : it->second) {
                sub->callback(&rbuf, msg.channel, sub->usr);
                wasDispatched = true;
            }
        }

        // dispatch to any regex channels
        for (auto& it : subsRegex) {
            for (auto& sub : it.second) {
                regex* r = (regex*)sub->regexobj;
                if (regex_match(msg.channel, *r)) {
                    sub->callback(&rbuf, msg.channel, sub->usr);
                    wasDispatched = true;
                }
            }
        }
    }

#ifdef TRACK_TRAFFIC_TOPOLOGY
    if (wasDispatched) {
        int64_t hashBE = 0, hashLE = 0;
        if (__int64_t_decode_array(msg.buf, 0, msg.len, &hashBE, 1) == 8 &&
            __int64_t_decode_little_endian_array(msg.buf, 0, msg.len, &hashLE, 1) == 8) {
            receivedTopologyMap[msg.channel].emplace(hashBE, hashLE);
        }
    }
#else
    (void)wasDispatched; // get rid of compiler warning
#endif
}

bool zcm_blocking_t::deleteSubEntry(zcm_sub_t* sub, size_t nentriesleft)
{
    int rc = ZCM_EOK;
    if (sub->regex) {
        delete (std::regex*) sub->regexobj;
    }
    if (nentriesleft == 0) {
        rc = zcm_trans_recvmsg_enable(zt, sub->channel, false);
    }
    delete sub;
    return rc == ZCM_EOK;
}

bool zcm_blocking_t::deleteFromSubList(SubList& slist, zcm_sub_t* sub)
{
    for (size_t i = 0; i < slist.size(); i++) {
        if (slist[i] == sub) {
            // shrink the array by moving the last element
            size_t last = slist.size() - 1;
            slist[i] = slist[last];
            slist.resize(last);
            // delete the element
            return deleteSubEntry(sub, slist.size());
        }
    }
    return false;
}

int zcm_blocking_t::writeTopology(string name)
{
    if (runState != RunState::NONE && runState != RunState::PAUSED) return ZCM_EUNKNOWN;
    return zcm::writeTopology(name, receivedTopologyMap, sentTopologyMap);
}

/////////////// C Interface Functions ////////////////
extern "C" {

int zcm_blocking_try_create(zcm_blocking_t** zcm, zcm_t* z, zcm_trans_t* trans)
{
    if (z->type != ZCM_BLOCKING) return ZCM_EINVALID;

    *zcm = new zcm_blocking_t(z, trans);
    if (!*zcm) return ZCM_EMEMORY;

    return ZCM_EOK;
}

void zcm_blocking_destroy(zcm_blocking_t* zcm)
{
    if (zcm) delete zcm;
}

int zcm_blocking_publish(zcm_blocking_t* zcm, const char* channel, const uint8_t* data, uint32_t len)
{
    return zcm->publish(channel, data, len);
}

zcm_sub_t* zcm_blocking_subscribe(zcm_blocking_t* zcm, const char* channel,
                                  zcm_msg_handler_t cb, void* usr)
{
    return zcm->subscribe(channel, cb, usr, true);
}

int zcm_blocking_unsubscribe(zcm_blocking_t* zcm, zcm_sub_t* sub)
{
    return zcm->unsubscribe(sub, true);
}

int zcm_blocking_flush(zcm_blocking_t* zcm)
{
    return zcm->flush();
}

void zcm_blocking_run(zcm_blocking_t* zcm)
{
    return zcm->run();
}

void zcm_blocking_start(zcm_blocking_t* zcm)
{
    return zcm->start();
}

void zcm_blocking_pause(zcm_blocking_t* zcm)
{
    return zcm->pause();
}

void zcm_blocking_resume(zcm_blocking_t* zcm)
{
    return zcm->resume();
}

void zcm_blocking_stop(zcm_blocking_t* zcm)
{
    return zcm->stop();
}

int zcm_blocking_handle(zcm_blocking_t* zcm, unsigned timeout)
{
    return zcm->handle(timeout);
}

int zcm_blocking_set_queue_size(zcm_blocking_t* zcm, unsigned num_messages)
{
    return zcm->setQueueSize(num_messages);
}

int  zcm_blocking_get_num_dropped_messages(zcm_blocking_t *zcm)
{
    return zcm->getNumDroppedMessages();
}

int zcm_blocking_write_topology(zcm_blocking_t* zcm, const char* name)
{
#ifdef TRACK_TRAFFIC_TOPOLOGY
    return zcm->writeTopology(string(name));
#endif
    return ZCM_EINVALID;
}



/****************************************************************************/
/*    NOT FOR GENERAL USE. USED FOR LANGUAGE-SPECIFIC BINDINGS WITH VERY    */
/*                     SPECIFIC THREADING CONSTRAINTS                       */
/****************************************************************************/
zcm_sub_t* zcm_blocking_try_subscribe(zcm_blocking_t* zcm, const char* channel,
                                      zcm_msg_handler_t cb, void* usr)
{
    return zcm->subscribe(channel, cb, usr, false);
}

int zcm_blocking_try_unsubscribe(zcm_blocking_t* zcm, zcm_sub_t* sub)
{
    return zcm->unsubscribe(sub, false);
}
/****************************************************************************/

}
