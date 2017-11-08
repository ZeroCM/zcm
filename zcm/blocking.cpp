#include "zcm/zcm.h"
#include "zcm/zcm_private.h"
#include "zcm/blocking.h"
#include "zcm/transport.h"
#include "zcm/util/threadsafe_queue.hpp"
#include "zcm/util/debug.h"

#include "util/TimeUtil.hpp"

#include <unistd.h>
#include <cassert>
#include <cstring>

#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <regex>
using namespace std;

#define RECV_TIMEOUT 100

// A C++ class that manages a zcm_msg_t*
struct Msg
{
    zcm_msg_t msg;

    // NOTE: copy the provided data into this object
    Msg(uint64_t utime, const char* channel, size_t len, const uint8_t* buf)
    {
        msg.utime = utime;
        msg.channel = strdup(channel);
        msg.len = len;
        msg.buf = (uint8_t*)malloc(len);
        memcpy(msg.buf, buf, len);
    }

    Msg(zcm_msg_t* msg) : Msg(msg->utime, msg->channel, msg->len, msg->buf) {}

    ~Msg()
    {
        if (msg.channel)
            free((void*)msg.channel);
        if (msg.buf)
            free((void*)msg.buf);
        memset(&msg, 0, sizeof(msg));
    }

    zcm_msg_t* get()
    {
        return &msg;
    }

  private:
    // Disable all copying and moving
    Msg(const Msg& other) = delete;
    Msg(Msg&& other) = delete;
    Msg& operator=(const Msg& other) = delete;
    Msg& operator=(Msg&& other) = delete;
};

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
    using SubList = vector<zcm_sub_t*>;

  public:
    zcm_blocking(zcm_t* z, zcm_trans_t* zt_);
    ~zcm_blocking();

    void run();
    void start();
    void stop();
    int handle();

    void pause();
    void resume();

    int publish(const string& channel, const uint8_t* data, uint32_t len);
    zcm_sub_t* subscribe(const string& channel, zcm_msg_handler_t cb, void* usr, bool block);
    int unsubscribe(zcm_sub_t* sub, bool block);
    void flush();

    void setRecvQueueSize(uint32_t numMsgs);

  private:
    void sendThreadFunc();
    void recvThreadFunc();
    void handleThreadFunc();

    void dispatchMsg(zcm_msg_t* msg);
    int handleOneMessage();

    bool deleteSubEntry(zcm_sub_t* sub, size_t nentriesleft);
    bool deleteFromSubList(SubList& slist, zcm_sub_t* sub);

  private:
    typedef enum {
        MODE_NONE = 0,
        MODE_RUN,
        MODE_SPAWN,
        MODE_HANDLE
    } Mode_t;

    zcm_t* z;
    zcm_trans_t* zt;
    unordered_map<string, SubList> subs;
    SubList subRegex;
    size_t mtu;

    Mode_t mode = MODE_NONE;

    thread sendThread;
    thread recvThread;
    thread handleThread;

    // RRR: might not need atomics here
    // Note: in order for their respective threads to shut down properly, the following MUST
    //       be set to false PRIOR to calling the forceWakeups() function on the queue the
    //       thread operates on. Else forceWakeups() may have to be called again to successfully
    //       wake the thread.
    atomic<bool> sendRunning   {false}; // operates on the sendQueue
    atomic<bool> recvRunning   {false}; // operates on the recvQueue
    atomic<bool> handleRunning {false}; // operates on the recvQueue

    static constexpr size_t QUEUE_SIZE = 16;
    ThreadsafeQueue<Msg> sendQueue {QUEUE_SIZE};
    ThreadsafeQueue<Msg> recvQueue {QUEUE_SIZE};

    // This mutex protects read and write access to the mode flag
    mutex              modeMutex;

    // These 3 mutexes are used around blocks of code in which you change the value of
    // any of the ...Running flags
    mutex              sendMutex;
    mutex              recvMutex;
    mutex              handleMutex;

    // These 2 mutexes used to implement a read-write style infrastructure on the subscription
    // lists. Both the recvThread and the message dispatch may read the subscriptions
    // concurrently, but subscribe() and unsubscribe() need both locks in order to write to it.
    mutex              subDispatchMutex;
    mutex              subRecvMutex;

    // Mutex protecting message dispatch (through the handleOne() func)
    mutex              dispatchMutex;

    bool               paused {false};
    mutex              pauseMutex;
    condition_variable pauseCond;
};

zcm_blocking_t::zcm_blocking(zcm_t* z, zcm_trans_t* zt_)
{
    zt = zt_;
    mtu = zcm_trans_get_mtu(zt);
}

zcm_blocking_t::~zcm_blocking()
{
    // Shutdown all threads
    stop();

    // Destroy the transport
    zcm_trans_destroy(zt);

    // Need to delete all subs
    for (auto& it : subs) {
        for (auto& sub : it.second) {
            delete sub;
        }
    }
    for (auto& sub : subRegex) {
        delete (regex*) sub->regexobj;
        delete sub;
    }
}

void zcm_blocking_t::run()
{
    unique_lock<mutex> lk1(modeMutex);
    if (mode != MODE_NONE) {
        ZCM_DEBUG("Err: call to run() when 'mode != MODE_NONE'");
        return;
    }
    mode = MODE_RUN;
    lk1.unlock();

    // Run it!
    {
        unique_lock<mutex> lk2(handleMutex);
        handleRunning = true;
    }
    handleThreadFunc();

    // Restore the "non-running" state
    lk1.lock();
    mode = MODE_NONE;
}

// TODO: should this call be thread safe?
void zcm_blocking_t::start()
{
    unique_lock<mutex> lk1(modeMutex);
    if (mode != MODE_NONE) {
        ZCM_DEBUG("Err: call to start() when 'mode != MODE_NONE'");
        return;
    }
    mode = MODE_SPAWN;

    unique_lock<mutex> lk2(handleMutex);
    // Start the handle thread
    handleRunning = true;
    handleThread = thread{&zcm_blocking::handleThreadFunc, this};
}

void zcm_blocking_t::stop()
{
    unique_lock<mutex> lk1(modeMutex);

    // Shutdown recv and handle threads
    if (mode == MODE_RUN || mode == MODE_SPAWN) {
        unique_lock<mutex> lk2(handleMutex);
        if (handleRunning) {
            handleRunning = false;
            pauseCond.notify_all();
            recvQueue.forceWakeups();
            if (mode == MODE_SPAWN) handleThread.join();
        }

    // Shutdown recv thread
    } else if (mode == MODE_HANDLE) {
        unique_lock<mutex> lk2(recvMutex);
        if (recvRunning) {
            recvRunning = false;
            recvQueue.forceWakeups();
            recvThread.join();
        }
    }

    // Shutdown send thread
    {
        unique_lock<mutex> lk(sendMutex);
        if (sendRunning) {
            sendRunning = false;
            sendQueue.forceWakeups();
            sendThread.join();
        }
    }

    // Restore the "non-running" state
    mode = MODE_NONE;
}

int zcm_blocking_t::handle()
{
    {
        unique_lock<mutex> lk(modeMutex);
        if (mode != MODE_NONE && mode != MODE_HANDLE) {
            ZCM_DEBUG("Err: call to handle() when 'mode != MODE_NONE && mode != MODE_HANDLE'");
            return -1;
        }

        // If this is the first time handle() is called, we need to start the recv thread
        if (mode == MODE_NONE) {
            // Spawn the recv thread
            recvRunning = true;
            recvThread = thread{&zcm_blocking::recvThreadFunc, this};
            mode = MODE_HANDLE;
        }
    }

    unique_lock<mutex> lk(dispatchMutex);
    return handleOneMessage();
}

void zcm_blocking_t::pause()
{
    unique_lock<mutex> lk(pauseMutex);
    paused = true;
    pauseCond.notify_all();
}

void zcm_blocking_t::resume()
{
    unique_lock<mutex> lk(pauseMutex);
    paused = false;
    pauseCond.notify_all();
}

// Note: We use a lock on publish() to make sure it can be
// called concurrently. Without the lock, there is a potential
// race to block on sendQueue.push()
int zcm_blocking_t::publish(const string& channel, const uint8_t* data, uint32_t len)
{
    // Check the validity of the request
    if (len > mtu) return ZCM_EINVALID;
    if (channel.size() > ZCM_CHANNEL_MAXLEN) return ZCM_EINVALID;


    // If needed: spawn the send thread
    if (!sendRunning) {
        unique_lock<mutex> lk(sendMutex);
        if (!sendRunning) {
            sendRunning = true;
            sendThread = thread{&zcm_blocking::sendThreadFunc, this};
        }
    }

    bool success = sendQueue.pushIfRoom(TimeUtil::utime(), channel.c_str(), len, data);
    if (!success) ZCM_DEBUG("sendQueue has no free space");
    return success ? ZCM_EOK : ZCM_EAGAIN;
}

// Note: We use a lock on subscribe() to make sure it can be
// called concurrently. Without the lock, there is a race
// on modifying and reading the 'subs' and 'subRegex' containers
zcm_sub_t* zcm_blocking_t::subscribe(const string& channel,
                                     zcm_msg_handler_t cb, void* usr,
                                     bool block)
{
    unique_lock<mutex> lk1(subDispatchMutex, std::defer_lock);
    unique_lock<mutex> lk2(subRecvMutex,     std::defer_lock);
    if (block) {
        lk1.lock();
        lk2.lock();
    } else if (!lk1.try_lock() || !lk2.try_lock()) {
        return nullptr;
    }
    int rc;

    bool regex = isRegexChannel(channel);
    if (regex) {
        if (subRegex.size() == 0) {
            rc = zcm_trans_recvmsg_enable(zt, NULL, true);
        } else {
            rc = ZCM_EOK;
        }
    } else {
        rc = zcm_trans_recvmsg_enable(zt, channel.c_str(), true);
    }

    if (rc != ZCM_EOK) {
        ZCM_DEBUG("zcm_trans_recvmsg_enable() didn't return ZCM_EOK: %d", rc);
        return nullptr;
    }

    zcm_sub_t* sub = new zcm_sub_t();
    ZCM_ASSERT(sub);
    strncpy(sub->channel, channel.c_str(), sizeof(sub->channel)/sizeof(sub->channel[0]));
    sub->regex = regex;
    sub->regexobj = nullptr;
    sub->callback = cb;
    sub->usr = usr;
    if (regex) {
        sub->regexobj = (void*) new std::regex(sub->channel);
        ZCM_ASSERT(sub->regexobj);
        subRegex.push_back(sub);
    } else {
        subs[channel].push_back(sub);
    }

    return sub;
}

// Note: We use a lock on unsubscribe() to make sure it can be
// called concurrently. Without the lock, there is a race
// on modifying and reading the 'subs' and 'subRegex' containers
int zcm_blocking_t::unsubscribe(zcm_sub_t* sub, bool block)
{
    unique_lock<mutex> lk1(subDispatchMutex, std::defer_lock);
    unique_lock<mutex> lk2(subRecvMutex,     std::defer_lock);
    if (block) {
        lk1.lock();
        lk2.lock();
    } else if (!lk1.try_lock() || !lk2.try_lock()) {
        return -2;
    }

    bool success = true;
    if (sub->regex) {
        success = deleteFromSubList(subRegex, sub);
    } else {
        auto it = subs.find(sub->channel);
        if (it == subs.end()) {
            ZCM_DEBUG("failed to find the subscription channel in unsubscribe()");
            return -1;
        }

        SubList& slist = it->second;
        success = deleteFromSubList(slist, sub);
    }

    if (!success) {
        ZCM_DEBUG("failed to find the subscription entry in unsubscribe()");
        return -1;
    }

    return 0;
}

void zcm_blocking_t::flush()
{
    {
        unique_lock<mutex> lk(dispatchMutex);
        size_t n = recvQueue.numMessages();
        for (size_t i = 0; i < n; ++i) handleOneMessage();
    }
    {
        // RRR: this won't prevent additional publishes anymore because we changed where pubMutex
        //      is used. Currently, this will block until all messages (including ones published
        //      after this function was called) are sent on the transport.
        sendQueue.waitForEmpty();
    }
}

void zcm_blocking_t::setRecvQueueSize(uint32_t numMsgs)
{
    recvQueue.resize(numMsgs);
}

void zcm_blocking_t::sendThreadFunc()
{
    while (sendRunning) {
        Msg* m = sendQueue.top();
        // If the Queue was forcibly woken-up, recheck the
        // running condition, and then retry.
        if (m == nullptr) continue;

        zcm_msg_t* msg = m->get();
        int ret = zcm_trans_sendmsg(zt,* msg);
        if (ret != ZCM_EOK) ZCM_DEBUG("zcm_trans_sendmsg() returned error, dropping the msg!");
        sendQueue.pop();
    }
}

void zcm_blocking_t::recvThreadFunc()
{
    while (recvRunning) {
        zcm_msg_t msg;
        int rc = zcm_trans_recvmsg(zt, &msg, RECV_TIMEOUT);
        if (rc == ZCM_EOK) {
            bool success;
            do {
                {
                    unique_lock<mutex> lk(subRecvMutex);

                    // Check if message matches a non regex channel
                    auto it = subs.find(msg.channel);
                    if (it == subs.end()) {
                        // Check if message matches a regex channel
                        bool foundRegex = false;
                        for (zcm_sub_t* sub : subRegex) {
                            regex* r = (regex*)sub->regexobj;
                            if (regex_match(msg.channel, *r)) {
                                foundRegex = true;
                                break;
                            }
                        }
                        // No subscription actually wants the message
                        if (!foundRegex) {
                            success = true;
                            continue;
                        }
                    }
                }

                // Note: push only fails if it was forcefully woken up. In such a case, we
                //       need to re-check the running condition; however, if we are still
                //       running, we want to still push the same message, necessitating the
                //       addition conditional on running.
                success = recvQueue.push(&msg);
            } while(!success && recvRunning);
        }
    }
}

void zcm_blocking_t::handleThreadFunc()
{
    {
        // Spawn the recv thread
        unique_lock<mutex> lk(recvMutex);
        recvRunning = true;
        recvThread = thread{&zcm_blocking::recvThreadFunc, this};
    }

    // Become the handle thread
    while (handleRunning) {
        {
            unique_lock<mutex> lk(pauseMutex);
            pauseCond.wait(lk, [&]{ return !paused || !handleRunning; });
            if (!handleRunning) break;
        }
        unique_lock<mutex> lk(dispatchMutex);
        handleOneMessage();
    }

    {
        // Shutdown recv thread
        unique_lock<mutex> lk(recvMutex);
        recvRunning = false;
        recvQueue.forceWakeups();
        recvThread.join();
    }
}

void zcm_blocking_t::dispatchMsg(zcm_msg_t* msg)
{
    zcm_recv_buf_t rbuf;
    rbuf.recv_utime = msg->utime;
    rbuf.zcm = z;
    rbuf.data = msg->buf;
    rbuf.data_size = msg->len;

    // Note: We use a lock on dispatch to ensure there is not
    // a race on modifying and reading the 'subs' container.
    // This means users cannot call zcm_subscribe or
    // zcm_unsubscribe from a callback without deadlocking.
    {
        unique_lock<mutex> lk(subDispatchMutex);

        // dispatch to a non regex channel
        auto it = subs.find(msg->channel);
        if (it != subs.end()) {
            for (zcm_sub_t* sub : it->second) {
                sub->callback(&rbuf, msg->channel, sub->usr);
            }
        }

        // dispatch to any regex channels
        for (zcm_sub_t* sub : subRegex) {
            regex* r = (regex*)sub->regexobj;
            if (regex_match(msg->channel, *r)) {
                sub->callback(&rbuf, msg->channel, sub->usr);
            }
        }
    }
}

int zcm_blocking_t::handleOneMessage()
{
    Msg* m = recvQueue.top();
    // If the Queue was forcibly woken-up, recheck the
    // running condition, and then retry.
    if (m == nullptr)
        return -1;

    dispatchMsg(m->get());
    recvQueue.pop();
    return 0;
}

bool zcm_blocking_t::deleteSubEntry(zcm_sub_t* sub, size_t nentriesleft)
{
    int rc = ZCM_EOK;
    if (sub->regex) {
        delete (std::regex*) sub->regexobj;
        if (nentriesleft == 0) {
            rc = zcm_trans_recvmsg_enable(zt, NULL, false);
        }
    } else {
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
            size_t last = slist.size()-1;
            slist[i] = slist[last];
            slist.resize(last);
            // delete the element
            return deleteSubEntry(sub, slist.size());
        }
    }
    return false;
}

/////////////// C Interface Functions ////////////////
extern "C" {

zcm_blocking_t* zcm_blocking_create(zcm_t* z, zcm_trans_t* trans)
{
    return new zcm_blocking_t(z, trans);
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

zcm_sub_t* zcm_blocking_try_subscribe(zcm_blocking_t* zcm, const char* channel,
                                      zcm_msg_handler_t cb, void* usr)
{
    return zcm->subscribe(channel, cb, usr, false);
}

int zcm_blocking_unsubscribe(zcm_blocking_t* zcm, zcm_sub_t* sub)
{
    return zcm->unsubscribe(sub, true);
}

int zcm_blocking_try_unsubscribe(zcm_blocking_t* zcm, zcm_sub_t* sub)
{
    return zcm->unsubscribe(sub, false);
}

void zcm_blocking_flush(zcm_blocking_t* zcm)
{
    zcm->flush();
}

void zcm_blocking_run(zcm_blocking_t* zcm)
{
    zcm->run();
}

void zcm_blocking_start(zcm_blocking_t* zcm)
{
    zcm->start();
}

void zcm_blocking_pause(zcm_blocking_t* zcm)
{
    zcm->pause();
}

void zcm_blocking_resume(zcm_blocking_t* zcm)
{
    zcm->resume();
}

void zcm_blocking_stop(zcm_blocking_t* zcm)
{
    zcm->stop();
}

int zcm_blocking_handle(zcm_blocking_t* zcm)
{
    return zcm->handle();
}

void zcm_blocking_set_recv_queue_size(zcm_blocking_t* zcm, uint32_t sz)
{
    return zcm->setRecvQueueSize(sz);
}

}
