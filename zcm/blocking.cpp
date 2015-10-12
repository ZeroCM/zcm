#include "zcm/zcm.h"
#include "zcm/blocking.h"
#include "zcm/transport.h"
#include "zcm/util/threadsafe_queue.hpp"
#include "zcm/util/debug.h"

#include <sys/time.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

#include <unordered_map>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
using namespace std;

#define RECV_TIMEOUT 100

// TODO: Should this be in a special library
namespace TimeUtil {
    static inline uint64_t utime()
    {
        struct timeval tv;
        gettimeofday (&tv, NULL);
        return (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
    }
}

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

struct Sub
{
    zcm_callback_t *callback;
    void *usr;
};

struct SubRegex
{
    string channelRegex;
    zcm_callback_t *callback;
    void *usr;
};

struct zcm_blocking
{
    zcm_t *z;
    zcm_trans_t *zt;
    unordered_map<string, Sub> subs;
    vector<SubRegex> subRegex;

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

    std::atomic<bool> sendRunning   {false};
    std::atomic<bool> recvRunning   {false};
    std::atomic<bool> handleRunning {false};

    static constexpr size_t QUEUE_SIZE = 16;
    ThreadsafeQueue<Msg> sendQueue {QUEUE_SIZE};
    ThreadsafeQueue<Msg> recvQueue {QUEUE_SIZE};

    mutex pubmut;
    mutex submut;

    zcm_blocking(zcm_t *z, zcm_trans_t *zt_)
    {
        zt = zt_;

        // Spawn the send thread
        sendRunning = true;
        sendThread = thread{&zcm_blocking::sendThreadFunc, this};
    }

    ~zcm_blocking()
    {
        if (mode == MODE_SPAWN) {
            stop();
        } else {
            // XXX nedd to do something with 'mode == MODE_BECOME'
        }

        // Shutdown send thread
        sendRunning = false;
        sendQueue.forceWakeups();
        sendThread.join();

        zcm_trans_destroy(zt);
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
                    success = recvQueue.push(&msg);
                    // XXX: I believe this could deadlock if the recvQueue gets
                    //      forcefully woken up (makes push always return false)
                } while(!success);
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

        // Note: We use a lock on dispatch to ensure there is not
        // a race on modifying and reading the 'subs' and
        // 'subRegex' containers. This means users cannot call
        // zcm_subscribe or zcm_unsubscribe from a callback without
        // deadlocking.
        {
            unique_lock<mutex> lk(submut);

            // dispatch to a non regex channel
            auto it = subs.find(msg->channel);
            if (it != subs.end()) {
                auto& sub = it->second;
                sub.callback(&rbuf, msg->channel, sub.usr);
            }

            // dispatch to any regex channels
            for (auto& sreg : subRegex) {
                if (sreg.channelRegex == ".*") {
                    sreg.callback(&rbuf, msg->channel, sreg.usr);
                } else {
                    // TODO: Implement more regex
                    ZCM_DEBUG("ZCM only supports the '.*' regex (aka subscribe-all)");
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
    int publish(const string& channel, const char *data, size_t len)
    {
        unique_lock<mutex> lk(pubmut);

        // TODO: publish should allow dropping of old messages
        if (!sendQueue.hasFreeSpace()) {
            ZCM_DEBUG("sendQueue has no free space");
            return -1;
        }

        bool success;
        do {
            success = sendQueue.push(channel.c_str(), len, data);
            // XXX: I believe this could deadlock if the sendQueue gets
            //      forcefully woken up (makes push always return false)
        } while(!success);

        return 0;
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
    int subscribe(const string& channel, zcm_callback_t *cb, void *usr)
    {
        unique_lock<mutex> lk(submut);
        int rc;

        if (isRegexChannel(channel)) {
            rc = zcm_trans_recvmsg_enable(zt, NULL, true);
            if (rc == ZCM_EOK)
                subRegex.emplace_back(SubRegex{channel, cb, usr});
        } else {
            rc = zcm_trans_recvmsg_enable(zt, channel.c_str(), true);
            if (rc == ZCM_EOK)
                subs.emplace(channel, Sub{cb, usr});
        }

        if (rc != ZCM_EOK) {
            ZCM_DEBUG("zcm_trans_recvmsg_enable() didn't return ZCM_EOK");
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

        // Shutdown recv thread
        recvRunning = false;
        recvQueue.forceWakeups();
        recvThread.join();

        // Shutdown handle thread
        handleRunning = false;
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

int zcm_blocking_publish(zcm_blocking_t *zcm, const char *channel, char *data, size_t len)
{
    return zcm->publish(channel, data, len);
}

int zcm_blocking_subscribe(zcm_blocking_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    return zcm->subscribe(channel, cb, usr);
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
