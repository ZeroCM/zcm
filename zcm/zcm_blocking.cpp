#include "zcm/zcm.h"
#include "zcm/zcm_blocking.h"
#include "zcm/transport.h"
#include "zcm/util/threadsafe_queue.hpp"
#include "zcm/util/debug.h"

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

    bool started = false;
    std::atomic<bool> running {true};

    thread sendThread;
    thread recvThread;
    thread handleThread;

    static constexpr size_t QUEUE_SIZE = 16;
    ThreadsafeQueue<Msg> sendQueue {QUEUE_SIZE};
    ThreadsafeQueue<Msg> recvQueue {QUEUE_SIZE};

    mutex pubmut;
    mutex submut;

    zcm_blocking(zcm_t *z, zcm_trans_t *zt_)
    {
        zt = zt_;
        start();
    }

    ~zcm_blocking()
    {
        stop();
        zcm_trans_destroy(zt);
    }

    void sendThreadFunc()
    {
        while (running) {
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
        while (running) {
            zcm_msg_t msg;
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
        rbuf.utime = 0;
        rbuf.len = msg->len;
        rbuf.data = (char*)msg->buf;

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

    void handleThreadFunc()
    {
        while (running) {
            Msg *m = recvQueue.top();
            // If the Queue was forcibly woken-up, recheck the
            // running condition, and then retry.
            if (m == nullptr)
                continue;
            dispatchMsg(m->get());
            recvQueue.pop();
        }
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

    int handle()
    {
        // TODO: impl
        while (1)
            usleep(1000000);
        return 0;
    }

    void become()
    {
        // This is a hack, we should use this thread for something productive
        while (1)
            usleep(1000*1000);
    }

    // TODO: should this call be thread safe?
    void start()
    {
        assert(!started);
        started = true;

        sendThread = thread{&zcm_blocking::recvThreadFunc, this};
        recvThread = thread{&zcm_blocking::sendThreadFunc, this};
        handleThread = thread{&zcm_blocking::handleThreadFunc, this};
    }

    void stop()
    {
        if (running && started) {
            running = false;
            sendQueue.forceWakeups();
            recvQueue.forceWakeups();
            handleThread.join();
            recvThread.join();
            sendThread.join();
        }
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

}
