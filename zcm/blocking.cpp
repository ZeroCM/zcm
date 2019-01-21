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
    int stop(bool block);
    int handle();

    void pause();
    void resume();

    int publish(const string& channel, const uint8_t* data, uint32_t len);
    zcm_sub_t* subscribe(const string& channel, zcm_msg_handler_t cb, void* usr, bool block);
    int unsubscribe(zcm_sub_t* sub, bool block);
    int flush(bool block);

    int setQueueSize(uint32_t numMsgs, bool block);

  private:
    void sendThreadFunc();
    void recvThreadFunc();
    void hndlThreadFunc();

    void dispatchMsg(zcm_msg_t* msg);
    bool dispatchOneMessage();
    bool sendOneMessage();

    // Mutexes protecting the ...OneMessage() functions
    mutex dispOneMutex;
    mutex sendOneMutex;

    bool deleteSubEntry(zcm_sub_t* sub, size_t nentriesleft);
    bool deleteFromSubList(SubList& slist, zcm_sub_t* sub);

    zcm_t* z;
    zcm_trans_t* zt;
    unordered_map<string, SubList> subs;
    SubList subRegex;
    size_t mtu;

    // These 2 mutexes used to implement a read-write style infrastructure on the subscription
    // lists. Both the recvThread and the message dispatch may read the subscriptions
    // concurrently, but subscribe() and unsubscribe() need both locks in order to write to it.
    mutex subDispMutex;
    mutex subRecvMutex;

    static constexpr size_t QUEUE_SIZE = 16;
    ThreadsafeQueue<Msg> sendQueue {QUEUE_SIZE};
    ThreadsafeQueue<Msg> recvQueue {QUEUE_SIZE};

    typedef enum {
        RECV_MODE_NONE = 0,
        RECV_MODE_RUN,
        RECV_MODE_SPAWN,
        RECV_MODE_HANDLE
    } RecvMode_t;
    RecvMode_t recvMode {RECV_MODE_NONE};

    // This mutex protects read and write access to the recv mode flag
    mutex recvModeMutex;

    thread sendThread;
    thread recvThread;
    thread hndlThread;

    typedef enum {
        THREAD_STATE_STOPPED = 0,
        THREAD_STATE_RUNNING,
        THREAD_STATE_HALTING,
        THREAD_STATE_HALTED,
    } ThreadState_t;

    ThreadState_t sendThreadState {THREAD_STATE_STOPPED};
    ThreadState_t recvThreadState {THREAD_STATE_STOPPED};
    ThreadState_t hndlThreadState {THREAD_STATE_STOPPED};

    // These mutexes protect read and write access to the ThreadState flags
    mutex sendStateMutex;
    mutex recvStateMutex;
    mutex hndlStateMutex;

    // Flag and condition variables used to pause the sendThread (use sendStateMutex)
    // and hndlThread (use hndlStateMutex)
    bool               paused {false};
    condition_variable sendPauseCond;
    condition_variable hndlPauseCond;
};

zcm_blocking_t::zcm_blocking(zcm_t* z, zcm_trans_t* zt_)
{
    zt = zt_;
    mtu = zcm_trans_get_mtu(zt);
}

zcm_blocking_t::~zcm_blocking()
{
    // Shutdown all threads
    stop(true);

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
    unique_lock<mutex> lk1(recvModeMutex);
    if (recvMode != RECV_MODE_NONE) {
        ZCM_DEBUG("Err: call to run() when 'recvMode != RECV_MODE_NONE'");
        return;
    }
    recvMode = RECV_MODE_RUN;

    // Run it!
    {
        unique_lock<mutex> lk2(hndlStateMutex);
        lk1.unlock();
        hndlThreadState = THREAD_STATE_RUNNING;
        recvQueue.enable();
    }
    hndlThreadFunc();

    // Restore the "non-running" state
    lk1.lock();
    recvMode = RECV_MODE_NONE;
}

// TODO: should this call be thread safe?
void zcm_blocking_t::start()
{
    unique_lock<mutex> lk1(recvModeMutex);
    if (recvMode != RECV_MODE_NONE) {
        ZCM_DEBUG("Err: call to start() when 'recvMode != RECV_MODE_NONE'");
        return;
    }
    recvMode = RECV_MODE_SPAWN;

    unique_lock<mutex> lk2(hndlStateMutex);
    lk1.unlock();
    // Start the hndl thread
    hndlThreadState = THREAD_STATE_RUNNING;
    recvQueue.enable();
    hndlThread = thread{&zcm_blocking::hndlThreadFunc, this};
}

int zcm_blocking_t::stop(bool block)
{
    unique_lock<mutex> lk1(recvModeMutex);

    // Shutdown recv and hndl threads
    if (recvMode == RECV_MODE_RUN || recvMode == RECV_MODE_SPAWN) {
        unique_lock<mutex> lk2(hndlStateMutex);
        if (hndlThreadState == THREAD_STATE_RUNNING) {
            hndlThreadState = THREAD_STATE_HALTING;
            recvQueue.disable();
            lk2.unlock();
            hndlPauseCond.notify_all();
            if (block && recvMode == RECV_MODE_SPAWN) {
                hndlThread.join();
                lk2.lock();
                hndlThreadState = THREAD_STATE_STOPPED;
            }
        }

    // Shutdown recv thread
    } else if (recvMode == RECV_MODE_HANDLE) {
        unique_lock<mutex> lk2(recvStateMutex);
        if (recvThreadState == THREAD_STATE_RUNNING) {
            recvThreadState = THREAD_STATE_HALTING;
            recvQueue.disable();
            lk2.unlock();
            if (block) {
                recvThread.join();
                lk2.lock();
                recvThreadState = THREAD_STATE_STOPPED;
            }
        }
    }

    // Shutdown send thread
    {
        unique_lock<mutex> lk2(sendStateMutex);
        if (sendThreadState == THREAD_STATE_RUNNING) {
            sendThreadState = THREAD_STATE_HALTING;
            sendQueue.disable();
            lk2.unlock();
            sendPauseCond.notify_all();
            if (block) {
                sendThread.join();
                lk2.lock();
                sendThreadState = THREAD_STATE_STOPPED;
            }
        }
    }

    if (!block) {
        switch (recvMode) {
            case RECV_MODE_SPAWN:
                {
                    unique_lock<mutex> lk2(hndlStateMutex);
                    if (hndlThreadState == THREAD_STATE_HALTING) return ZCM_EAGAIN;
                    if (hndlThreadState == THREAD_STATE_HALTED) {
                        hndlThread.join();
                        hndlThreadState = THREAD_STATE_STOPPED;
                    }
                }
                break;
            case RECV_MODE_HANDLE:
                {
                    unique_lock<mutex> lk2(recvStateMutex);
                    if (recvThreadState == THREAD_STATE_HALTING) return ZCM_EAGAIN;
                    if (recvThreadState == THREAD_STATE_HALTED) {
                        recvThread.join();
                        recvThreadState = THREAD_STATE_STOPPED;
                    }
                }
                break;
            default: break;
        }

        unique_lock<mutex> lk2(sendStateMutex);
        if (sendThreadState == THREAD_STATE_HALTING) return ZCM_EAGAIN;
        if (sendThreadState == THREAD_STATE_HALTED) {
            sendThread.join();
            sendThreadState = THREAD_STATE_STOPPED;
        }
    }

    // Restore the "non-running" state
    recvMode = RECV_MODE_NONE;
    return ZCM_EOK;
}

int zcm_blocking_t::handle()
{
    {
        unique_lock<mutex> lk1(recvModeMutex);
        if (recvMode != RECV_MODE_NONE && recvMode != RECV_MODE_HANDLE) {
            ZCM_DEBUG("Err: call to handle() when 'recvMode != RECV_MODE_NONE && recvMode != RECV_MODE_HANDLE'");
            return ZCM_EINVALID;
        }

        // If this is the first time handle() is called, we need to start the recv thread
        if (recvMode == RECV_MODE_NONE) {
            recvMode = RECV_MODE_HANDLE;

            unique_lock<mutex> lk2(recvStateMutex);
            lk1.unlock();
            // Spawn the recv thread
            recvThreadState = THREAD_STATE_RUNNING;
            recvQueue.enable();
            recvThread = thread{&zcm_blocking::recvThreadFunc, this};
        }
    }

    unique_lock<mutex> lk(dispOneMutex);
    return dispatchOneMessage() ? ZCM_EOK : ZCM_EAGAIN;
}

void zcm_blocking_t::pause()
{
    unique_lock<mutex> lk1(sendStateMutex);
    unique_lock<mutex> lk2(hndlStateMutex);
    paused = true;
}

void zcm_blocking_t::resume()
{
    unique_lock<mutex> lk1(sendStateMutex);
    unique_lock<mutex> lk2(hndlStateMutex);
    paused = false;
    lk2.unlock();
    lk1.unlock();
    sendPauseCond.notify_all();
    hndlPauseCond.notify_all();
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
    {
        unique_lock<mutex> lk(sendStateMutex);
        if (sendThreadState == THREAD_STATE_STOPPED) {
            sendThreadState = THREAD_STATE_RUNNING;
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
    unique_lock<mutex> lk1(subDispMutex, std::defer_lock);
    unique_lock<mutex> lk2(subRecvMutex, std::defer_lock);
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
    strncpy(sub->channel, channel.c_str(), ZCM_CHANNEL_MAXLEN);
    sub->channel[ZCM_CHANNEL_MAXLEN] = '\0';
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
    unique_lock<mutex> lk1(subDispMutex, std::defer_lock);
    unique_lock<mutex> lk2(subRecvMutex, std::defer_lock);
    if (block) {
        lk1.lock();
        lk2.lock();
    } else if (!lk1.try_lock() || !lk2.try_lock()) {
        return ZCM_EAGAIN;
    }

    bool success = true;
    if (sub->regex) {
        success = deleteFromSubList(subRegex, sub);
    } else {
        auto it = subs.find(sub->channel);
        if (it == subs.end()) {
            ZCM_DEBUG("failed to find the subscription channel in unsubscribe()");
            return ZCM_EINVALID;
        }

        SubList& slist = it->second;
        success = deleteFromSubList(slist, sub);
    }

    if (!success) {
        ZCM_DEBUG("failed to find the subscription entry in unsubscribe()");
        return ZCM_EINVALID;
    }

    return 0;
}

int zcm_blocking_t::flush(bool block)
{
    size_t n;

    {
        sendQueue.disable();

        unique_lock<mutex> lk(sendOneMutex, defer_lock);

        if (block) lk.lock();
        else if (!lk.try_lock()) {
            sendQueue.enable();
            return ZCM_EAGAIN;
        }

        sendQueue.enable();
        n = sendQueue.numMessages();
        for (size_t i = 0; i < n; ++i) sendOneMessage();
    }

    {
        recvQueue.disable();

        unique_lock<mutex> lk(dispOneMutex, defer_lock);

        if (block) lk.lock();
        else if (!lk.try_lock()) {
            recvQueue.enable();
            return ZCM_EAGAIN;
        }

        recvQueue.enable();
        n = recvQueue.numMessages();
        for (size_t i = 0; i < n; ++i) dispatchOneMessage();
    }

    return ZCM_EOK;
}

int zcm_blocking_t::setQueueSize(uint32_t numMsgs, bool block)
{
    if (sendQueue.getCapacity() != numMsgs) {

        sendQueue.disable();

        unique_lock<mutex> lk(sendOneMutex, defer_lock);

        if (block) lk.lock();
        else if (!lk.try_lock()) {
            sendQueue.enable();
            return ZCM_EAGAIN;
        }

        sendQueue.setCapacity(numMsgs);
        sendQueue.enable();
    }

    if (recvQueue.getCapacity() != numMsgs) {

        recvQueue.disable();

        unique_lock<mutex> lk(dispOneMutex, defer_lock);

        if (block) lk.lock();
        else if (!lk.try_lock()) {
            recvQueue.enable();
            return ZCM_EAGAIN;
        }

        recvQueue.setCapacity(numMsgs);
        recvQueue.enable();
    }

    return ZCM_EOK;
}

void zcm_blocking_t::sendThreadFunc()
{
    while (true) {
        {
            unique_lock<mutex> lk(sendStateMutex);
            sendPauseCond.wait(lk, [&]{
                    return !paused || sendThreadState == THREAD_STATE_HALTING;
            });
            if (sendThreadState == THREAD_STATE_HALTING) break;
        }
        unique_lock<mutex> lk(sendOneMutex);
        sendOneMessage();
    }

    unique_lock<mutex> lk(sendStateMutex);
    sendThreadState = THREAD_STATE_HALTED;
}

void zcm_blocking_t::recvThreadFunc()
{
    while (true) {
        {
            unique_lock<mutex> lk(recvStateMutex);
            if (recvThreadState == THREAD_STATE_HALTING) break;
        }
        zcm_msg_t msg;
        int rc = zcm_trans_recvmsg(zt, &msg, RECV_TIMEOUT);
        if (rc == ZCM_EOK) {
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
                    if (!foundRegex) continue;
                }
            }

            // Note: After this returns, you have either successfully pushed a message
            //       into the queue, or the queue was disabled and you will quit out of
            //       this loop when you re-check the running condition
            recvQueue.push(&msg);
        }
    }
    unique_lock<mutex> lk(recvStateMutex);
    recvThreadState = THREAD_STATE_HALTED;
}

void zcm_blocking_t::hndlThreadFunc()
{
    {
        // Spawn the recv thread
        unique_lock<mutex> lk(recvStateMutex);
        recvThreadState = THREAD_STATE_RUNNING;
        recvThread = thread{&zcm_blocking::recvThreadFunc, this};
    }

    // Become the handle thread
    while (true) {
        {
            unique_lock<mutex> lk(hndlStateMutex);
            hndlPauseCond.wait(lk, [&]{
                return !paused || hndlThreadState == THREAD_STATE_HALTING;
            });
            if (hndlThreadState == THREAD_STATE_HALTING) break;
        }
        unique_lock<mutex> lk(dispOneMutex);
        dispatchOneMessage();
    }

    {
        // Shutdown recv thread
        unique_lock<mutex> lk(recvStateMutex);
        recvThreadState = THREAD_STATE_HALTING;
        recvQueue.disable();
        lk.unlock();
        recvThread.join();
    }

    unique_lock<mutex> lk(hndlStateMutex);
    hndlThreadState = THREAD_STATE_HALTED;
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
        unique_lock<mutex> lk(subDispMutex);

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

bool zcm_blocking_t::dispatchOneMessage()
{
    Msg* m = recvQueue.top();
    // If the Queue was forcibly woken-up, recheck the
    // running condition, and then retry.
    if (m == nullptr) return false;

    dispatchMsg(m->get());
    recvQueue.pop();
    return true;
}

bool zcm_blocking_t::sendOneMessage()
{
    Msg* m = sendQueue.top();
    // If the Queue was forcibly woken-up, recheck the
    // running condition, and then retry.
    if (m == nullptr) return false;

    zcm_msg_t* msg = m->get();
    int ret = zcm_trans_sendmsg(zt, *msg);
    if (ret != ZCM_EOK) ZCM_DEBUG("zcm_trans_sendmsg() returned error, dropping the msg!");
    sendQueue.pop();
    return true;
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
    zcm->flush(true);
}

int  zcm_blocking_try_flush(zcm_blocking_t* zcm)
{
    return zcm->flush(false);
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
    zcm->stop(true);
}

int  zcm_blocking_try_stop(zcm_blocking_t* zcm)
{
    return zcm->stop(false);
}

int zcm_blocking_handle(zcm_blocking_t* zcm)
{
    return zcm->handle();
}

void zcm_blocking_set_queue_size(zcm_blocking_t* zcm, uint32_t sz)
{
    zcm->setQueueSize(sz, true);
}

int  zcm_blocking_try_set_queue_size(zcm_blocking_t* zcm, uint32_t sz)
{
    return zcm->setQueueSize(sz, false);
}

}
