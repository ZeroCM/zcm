#include "zcm.h"
#include "transport_zmq_ipc.h"
#include "threadsafe_queue.hpp"

#include <unistd.h>
#include <cassert>
#include <cstring>

#include <unordered_map>
#include <condition_variable>
#include <iostream>
#include <thread>
using namespace std;

// A C++ class that manages a zcm_msg_t
struct Msg
{
    zcm_msg_t msg;

    Msg(zcm_msg_t m)
    {
        msg.channel = strdup(m.channel);
        msg.len = m.len;
        msg.buf = (char*)malloc(msg.len);
        memcpy(msg.buf, m.buf, msg.len);
    }

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
    zcm_callback_t *cb;
    void *usr;
};

struct zcm_t
{
    zcm_trans_t *zt;
    unordered_map<string, Sub> subs;

    thread sendThread;
    thread recvThread;

    static constexpr size_t QUEUE_SIZE = 16;
    ThreadsafeQueue<Msg> sendQueue {QUEUE_SIZE};
    ThreadsafeQueue<Msg> recvQueue {QUEUE_SIZE};

    zcm_t()
    {
        zt = zcm_trans_ipc_create();
        startThreads();
    }

    void startThreads()
    {
        sendThread = thread{&zcm_t::recvThreadFunc, this};
        recvThread = thread{&zcm_t::sendThreadFunc, this};
    }

    void recvThreadFunc()
    {
        while (true) {
            zcm_msg_t *msg = sendQueue.top().get();
            int ret = zcm_trans_sendmsg(zt, *msg);
            assert(ret == ZCM_EOK);
            sendQueue.pop();
        }
    }

    void sendThreadFunc()
    {
        while (true) {
            zcm_msg_t msg;
            int rc = zcm_trans_recvmsg(zt, &msg, 100);
            if (rc == ZCM_EOK)
                recvQueue.push(msg);
        }
    }

    int publish(const string& channel, const char *data, size_t len)
    {
        if (!sendQueue.hasFreeSpace())
            return 1;

        zcm_msg_t msg;
        msg.channel = channel.c_str();
        msg.len = len;
        msg.buf = (char*)data;
        sendQueue.push(msg);
        return 0;
    }


    int subscribe(const string& channel, zcm_callback_t *cb, void *usr)
    {
        zcm_trans_recvmsg_enable(zt, channel.c_str(), true);
        subs.emplace(channel, Sub{cb, usr});
        return 0;
    }

    void dispatchMsg(zcm_msg_t *msg)
    {
        auto it = subs.find(msg->channel);
        if (it != subs.end()) {
            auto& sub = it->second;
            zcm_recv_buf_t rbuf;
            rbuf.zcm = this;
            rbuf.utime = 0;
            rbuf.len = msg->len;
            rbuf.data = (char*)msg->buf;
            sub.cb(&rbuf, msg->channel, sub.usr);
        }
    }

    int handle()
    {
        auto& msg = recvQueue.top();
        dispatchMsg(msg.get());
        recvQueue.pop();
        return 0;
    }
};

/////////////// C Interface Functions ////////////////

zcm_t *zcm_create(void)
{
    return new zcm_t();
}

void zcm_destroy(zcm_t *zcm)
{
    if (zcm) delete zcm;
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    return zcm->publish(channel, data, len);
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    return zcm->subscribe(channel, cb, usr);
}

int zcm_handle(zcm_t *zcm)
{
    return zcm->handle();
}

int zcm_poll(zcm_t *zcm, uint ms)
{
    assert(0 && "deprecated!");
}
