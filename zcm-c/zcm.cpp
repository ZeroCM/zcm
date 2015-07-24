#include "zcm.h"
#include "transport_zmq_ipc.h"

#include <unistd.h>
#include <cassert>
#include <cstring>

#include <unordered_map>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <thread>
using namespace std;

#define QUEUE_SIZE 16
struct MsgQueue
{
    zcm_msg_t q[QUEUE_SIZE];
    size_t front = 0;
    size_t back = 0;

    mutex mut;
    condition_variable cond;

    static size_t incIdx(size_t i)
    {
        return (i+1) % QUEUE_SIZE;
    }

    bool _hasFreeSpaceNoLock()
    {
        size_t nextBack = incIdx(back);
        return nextBack != front;
    }

    bool hasFreeSpace()
    {
        unique_lock<mutex> lk(mut);
        return _hasFreeSpaceNoLock();
    }

    bool _hasMessageNoLock()
    {
        return front != back;
    }

    bool hasMessage()
    {
        unique_lock<mutex> lk(mut);
        return _hasFreeSpaceNoLock();
    }

    void push(zcm_msg_t msg)
    {
        unique_lock<mutex> lk(mut);
        cond.wait(lk, [&](){ return _hasFreeSpaceNoLock(); });

        zcm_msg_t *elt = &q[back];
        elt->channel = strdup(msg.channel);
        elt->len = msg.len;
        elt->buf = (char*)malloc(msg.len);
        memcpy(elt->buf, msg.buf, msg.len);
        back = incIdx(back);

        cond.notify_one();
    }

    zcm_msg_t *top()
    {
        unique_lock<mutex> lk(mut);
        cond.wait(lk, [&](){ return _hasMessageNoLock(); });

        return &q[front];
    }

    void pop()
    {
        unique_lock<mutex> lk(mut);

        zcm_msg_t *elt = &q[front];
        free((void*)elt->channel);
        free((void*)elt->buf);
        front = incIdx(front);
        cond.notify_one();
    }
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

    thread pubThread;
    thread subThread;

    MsgQueue recvQueue;
    MsgQueue sendQueue;

    zcm_t()
    {
        zt = zcm_trans_ipc_create();
        startThreads();
    }

    void startThreads()
    {
        pubThread = thread{&zcm_t::pubThreadFunc, this};
        subThread = thread{&zcm_t::subThreadFunc, this};
    }

    void pubThreadFunc()
    {
        while (true) {
            zcm_msg_t *msg = sendQueue.top();
            zcm_trans_sendmsg(zt, *msg);
            //assert(ret == ZCM_EOK);
            sendQueue.pop();
        }
    }

    void subThreadFunc()
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
        zcm_msg_t *msg = recvQueue.top();
        dispatchMsg(msg);
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
