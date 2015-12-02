#include "zcm/zcm.h"
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
#include <atomic>
#include <regex>
using namespace std;

#define RECV_TIMEOUT 100

// A C++ class that manages a zcm_msg_t
struct Msg
{
    zcm_msg_t msg;

    // NOTE: copy the provided data into this object
    Msg(const char *channel, size_t len, const char *buf)
    {
        msg.channel = strdup(channel);
        msg.len = len;
        msg.buf = (char*)malloc(len);
        memcpy(msg.buf, buf, len);
    }

    Msg(zcm_msg_t *msg) : Msg(msg->channel, msg->len, msg->buf) {}

    ~Msg()
    {
        if (msg.channel)
            free((void*)msg.channel);
        if (msg.buf)
            free((void*)msg.buf);
        memset(&msg, 0, sizeof(msg));
    }

    zcm_msg_t *get()
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

struct zcm_blocking
{
    zcm_t *z;
    zcm_trans_t *zt;
    unordered_multimap<string, zcm_sub_t> subs;
    vector<zcm_sub_t> subRegex;
    size_t mtu;

    typedef enum {
        MODE_NONE = 0,
        MODE_BECOME,
        MODE_SPAWN,
        MODE_HANDLE
    } Mode_t;

    Mode_t mode = MODE_NONE;

    thread sendThread;
    thread recvThread;
    thread handleThread;

    // Note: in order for their respective threads to shut down properly, the following MUST
    //       be set to false PRIOR to calling the forceWakeups() function on the queue the
    //       thread operates on. Else forceWakeups() may have to be called again to successfully
    //       wake the thread.
    std::atomic<bool> sendRunning   {false}; // operates on the sendQueue
    std::atomic<bool> recvRunning   {false}; // operates on the recvQueue
    std::atomic<bool> handleRunning {false}; // operates on the recvQueue

    static constexpr size_t QUEUE_SIZE = 16;
    ThreadsafeQueue<Msg> sendQueue {QUEUE_SIZE};
    ThreadsafeQueue<Msg> recvQueue {QUEUE_SIZE};

    mutex pubmut;
    mutex submut;

    zcm_blocking(zcm_t *z, zcm_trans_t *zt_)
    {
        zt = zt_;
        mtu = zcm_trans_get_mtu(zt);

        // Spawn the send thread
        sendRunning = true;
        sendThread = thread{&zcm_blocking::sendThreadFunc, this};
    }

    ~zcm_blocking()
    {
        if (mode == MODE_SPAWN) {
            stop();
        } else {
            // XXX need to do something with 'mode == MODE_BECOME'
        }

        // Shutdown send thread
        sendRunning = false;
        sendQueue.forceWakeups();
        sendThread.join();

        zcm_trans_destroy(zt);

        // Need to delete all regex objects
        for (auto& sub : subRegex) {
            delete (regex *) sub.regexobj;
        }
    }

    void sendThreadFunc()
    {
        while (sendRunning) {
            Msg *m = sendQueue.top();
            // If the Queue was forcibly woken-up, recheck the
            // running condition, and then retry.
            if (m == nullptr)
                continue;

            zcm_msg_t *msg = m->get();
            int ret = zcm_trans_sendmsg(zt, *msg);
            if (ret != ZCM_EOK)
                ZCM_DEBUG("zcm_trans_sendmsg() failed to return EOK.. dropping the msg!");
            sendQueue.pop();
        }
    }

    void recvThreadFunc()
    {
        while (recvRunning) {
            zcm_msg_t msg;
            // XXX remove this memset once transport layers know about the utime field
            memset(&msg, 0, sizeof(msg));
            int rc = zcm_trans_recvmsg(zt, &msg, RECV_TIMEOUT);
            if (rc == ZCM_EOK) {
                bool success;
                do {
                    // Note: push only fails if it was forcefully woken up. In such a case, we
                    //       need to re-check the running condition; however, if we are still
                    //       running, we want to still push the same message, necessitating the
                    //       addition conditional on running.
                    success = recvQueue.push(&msg);
                } while(!success && recvRunning);
            }
        }
    }

    void dispatchMsg(zcm_msg_t *msg)
    {
        zcm_recv_buf_t rbuf;
        rbuf.zcm = z;
        rbuf.data = (char*)msg->buf;
        rbuf.data_size = msg->len;
        rbuf.recv_utime = TimeUtil::utime();

        // XXX: the following needs to make it into high level docs:
        // Note: We use a lock on dispatch to ensure there is not
        // a race on modifying and reading the 'subs' and
        // 'subRegex' containers. This means users cannot call
        // zcm_subscribe or zcm_unsubscribe from a callback without
        // deadlocking.
        {
            unique_lock<mutex> lk(submut);

            // dispatch to a non regex channel
            auto its = subs.equal_range(msg->channel);
            for (auto it = its.first; it != its.second; ++it) {
                auto& sub = it->second;
                sub.callback(&rbuf, msg->channel, sub.usr);
            }

            // dispatch to any regex channels
            for (auto& sub : subRegex) {
                regex *r = (regex *)sub.regexobj;
                if (regex_match(msg->channel, *r)) {
                    sub.callback(&rbuf, msg->channel, sub.usr);
                }
            }
        }
    }

    int handleOneMessage()
    {
        Msg *m = recvQueue.top();
        // If the Queue was forcibly woken-up, recheck the
        // running condition, and then retry.
        if (m == nullptr)
            return -1;

        dispatchMsg(m->get());
        recvQueue.pop();
        return 0;
    }

    void handleThreadFunc()
    {
        while (handleRunning)
            handleOneMessage();
    }

    // Note: We use a lock on publish() to make sure it can be
    // called concurrently. Without the lock, there is a potential
    // race to block on sendQueue.push()
    int publish(const string& channel, const char *data, uint32_t len)
    {
        // Check the validity of the request
        if (len > mtu) return ZCM_EINVALID;
        if (channel.size() > ZCM_CHANNEL_MAXLEN) return ZCM_EINVALID;

        unique_lock<mutex> lk(pubmut);

        // TODO: publish should allow dropping of old messages
        if (!sendQueue.hasFreeSpace()) {
            ZCM_DEBUG("sendQueue has no free space");
            return ZCM_EAGAIN;
        }

        // Note: push only fails if it was forcefully woken up, which means zcm is shutting down
        bool success = sendQueue.push(channel.c_str(), len, data);
        return success ? ZCM_EOK : ZCM_EINTR;
    }

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

    // Note: We use a lock on subscribe() to make sure it can be
    // called concurrently. Without the lock, there is a race
    // on modifying and reading the 'subs' and 'subRegex' containers
    zcm_sub_t *subscribe(const string& channel, zcm_msg_handler_t cb, void *usr)
    {
        unique_lock<mutex> lk(submut);
        int rc;
        zcm_sub_t  sub;
        zcm_sub_t *retptr = NULL;

        bool regex = isRegexChannel(channel);

        if (regex) {
            // TODO: this is dependenton all regex being ".*"
            rc = zcm_trans_recvmsg_enable(zt, NULL, true);
        } else {
            rc = zcm_trans_recvmsg_enable(zt, channel.c_str(), true);
        }

        if (rc == ZCM_EOK) {
            strncpy(sub.channel, channel.c_str(), sizeof(sub.channel)/sizeof(sub.channel[0]));
            sub.regex = regex;
            sub.regexobj = nullptr;
            sub.callback = cb;
            sub.usr = usr;
            if (regex) {
                // XXX this is wrong. std::vector can be re-allocated and the pointer can be invalidated
                sub.regexobj = (void *) new std::regex(sub.channel);
                subRegex.emplace_back(forward<zcm_sub_t>(sub));
                retptr = &subRegex.back();
            } else {
                // XXX this is wrong. std::unordered_multimap can be re-allocated and the pointer can be invalidated
                auto it = subs.emplace(channel, forward<zcm_sub_t>(sub));
                retptr = &it->second;
            }
        }

        if (rc != ZCM_EOK) {
            ZCM_DEBUG("zcm_trans_recvmsg_enable() didn't return ZCM_EOK: %d", rc);
        }

        return retptr;
    }

    // Note: We use a lock on unsubscribe() to make sure it can be
    // called concurrently. Without the lock, there is a race
    // on modifying and reading the 'subs' and 'subRegex' containers
    int unsubscribe(zcm_sub_t *sub)
    {
        unique_lock<mutex> lk(submut);
        int rc = ZCM_EOK;

        if (sub->regex) {
            for (auto it = subRegex.begin(); it != subRegex.end(); ++it) {
                if (&(*it) == sub) {
                    subRegex.erase(it);
                    break;
                }
            }
            // XXX: this is dependent on all regex being ".*" (only those are allowed as of now)
            if (subRegex.empty()) {
                rc = zcm_trans_recvmsg_enable(zt, NULL, false);
            }
        } else {
            string channel = sub->channel;
            auto its = subs.equal_range(channel);
            for (auto it = its.first; it != its.second;) {
                if (sub == &it->second) {
                    it = subs.erase(it);
                } else {
                    ++it;
                }
            }
            if (subs.count(channel) == 0) {
                rc = zcm_trans_recvmsg_enable(zt, channel.c_str(), false);
            }
        }

        if (rc != ZCM_EOK) {
            ZCM_DEBUG("zcm_trans_recvmsg_enable() didn't return ZCM_EOK: %d", rc);
            return -1;
        }

        return 0;
    }

    void become()
    {
        if (mode != MODE_NONE) {
            ZCM_DEBUG("Err: call to become() when 'mode != MODE_NONE'");
            return;
        }
        mode = MODE_BECOME;

        // Spawn the recv thread
        recvRunning = true;
        recvThread = thread{&zcm_blocking::recvThreadFunc, this};

        // Become the handle/dispatch thread
        handleRunning = true;
        handleThreadFunc();
        // TODO: should add a function that can wake up the thread that "became" the zcm
        //       thread and trigger it to stop. Might even just extend the functionality of
        //       zcm_stop
        assert(!handleRunning);

        // Shutdown recv thread
        recvRunning = false;
        recvQueue.forceWakeups();
        recvThread.join();

        // Restore the "non-running" state
        mode = MODE_NONE;
    }

    // TODO: should this call be thread safe?
    void start()
    {
        if (mode != MODE_NONE) {
            ZCM_DEBUG("Err: call to start() when 'mode != MODE_NONE'");
            return;
        }
        mode = MODE_SPAWN;

        // Spawn the recv thread
        recvRunning = true;
        recvThread = thread{&zcm_blocking::recvThreadFunc, this};

        // Spawn the handle thread
        handleRunning = true;
        handleThread = thread{&zcm_blocking::handleThreadFunc, this};
    }

    void stop()
    {
        if (mode != MODE_SPAWN) {
            ZCM_DEBUG("Err: call to stop() when 'mode != MODE_SPAWN'");
            return;
        }

        // Shutdown recv and handle threads
        recvRunning = false;
        handleRunning = false;

        recvQueue.forceWakeups();

        recvThread.join();
        handleThread.join();

        // Restore the "non-running" state
        mode = MODE_NONE;
    }

    int handle()
    {
        if (mode != MODE_NONE && mode != MODE_HANDLE) {
            ZCM_DEBUG("Err: call to handle() when 'mode != MODE_SNODE && mode != MODE_HANDLE'");
            return -1;
        }

        // if this is the first time handle() is called, we need to start the recv thread
        if (mode == MODE_NONE) {
            // Spawn the recv thread
            recvRunning = true;
            recvThread = thread{&zcm_blocking::recvThreadFunc, this};
            mode = MODE_HANDLE;
        }

        return handleOneMessage();
    }
};

/////////////// C Interface Functions ////////////////
extern "C" {

zcm_blocking_t *zcm_blocking_create(zcm_t *z, zcm_trans_t *trans)
{
    return new zcm_blocking_t(z, trans);
}

void zcm_blocking_destroy(zcm_blocking_t *zcm)
{
    if (zcm) delete zcm;
}

int zcm_blocking_publish(zcm_blocking_t *zcm, const char *channel, const char *data, uint32_t len)
{
    return zcm->publish(channel, data, len);
}

zcm_sub_t *zcm_blocking_subscribe(zcm_blocking_t *zcm, const char *channel, zcm_msg_handler_t cb,
                                  void *usr)
{
    return zcm->subscribe(channel, cb, usr);
}

int zcm_blocking_unsubscribe(zcm_blocking_t *zcm, zcm_sub_t *sub)
{
    return zcm->unsubscribe(sub);
}

void zcm_blocking_become(zcm_blocking_t *zcm)
{
    zcm->become();
}

void zcm_blocking_start(zcm_blocking_t *zcm)
{
    zcm->start();
}

void zcm_blocking_stop(zcm_blocking_t *zcm)
{
    zcm->stop();
}

int zcm_blocking_handle(zcm_blocking_t *zcm)
{
    return zcm->handle();
}

}
