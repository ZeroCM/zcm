#include "zcm.h"
#include <zmq.h>

#include <unistd.h>

#include <cstring>
#include <cassert>
#include <cctype>
#include <cstdio>

#include <unordered_map>
using std::unordered_map;

#include <string>
using std::string;

#include <mutex>
#include <thread>
#include <iostream>

#define METADATA_PUB_ADDR "ipc:///tmp/zcm-metadata-pub"
#define METADATA_SUB_ADDR "ipc:///tmp/zcm-metadata-sub"
#define ZMQ_IO_THREADS 1

struct Sub
{
    zcm_callback_t *cb;
    void *usr;
};

struct zcm
{
    std::thread recvThread;

    // The mutex protects all data below it
    std::mutex mut;
    bool running = true;
    unordered_map<string, Sub> subs;

    void *ctx;
    unordered_map<string, void*> subsocks;
    unordered_map<string, void*> pubsocks;

    // TEMPORARY
    string subchannel;
    void *subsock;

    zcm()
    {
        ctx = zmq_init(ZMQ_IO_THREADS);
    }

    string _getAddress(const string& channel)
    {
        return "ipc:///tmp/zmq-channel-"+channel;
    }

    void *_getSubSock(const string& channel)
    {
        auto it = subsocks.find(channel);
        if (it != subsocks.end())
            return it->second;
        void *sock = zmq_socket(ctx, ZMQ_SUB);
        string address = _getAddress(channel);
        zmq_connect(sock, address.c_str());
        zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
        subsocks[channel] = sock;

        // TEMPORARY
        subchannel = channel;
        subsock = sock;

        return sock;
    }

    void *_getPubSock(const string& channel)
    {
        auto it = pubsocks.find(channel);
        if (it != pubsocks.end())
            return it->second;
        void *sock = zmq_socket(ctx, ZMQ_PUB);
        string address = _getAddress(channel);
        zmq_bind(sock, address.c_str());
        pubsocks[channel] = sock;
        return sock;
    }

    int publish(const string& channel, const char *data, size_t len)
    {
        std::unique_lock<std::mutex> lk(mut);

        void *sock = _getPubSock(channel);
        int rc = zmq_send(sock, data, len, 0);
        assert(rc == (int)len);
        return 0;
    }

    void recvThreadFunc()
    {
        const int BUFSZ = 1 << 20;
        char *buf = (char *) malloc(BUFSZ);

        while (1) {
            int rc = zmq_recv(subsock, buf, BUFSZ, 0);
            assert(0 < rc && rc < BUFSZ);

            // dispatch
            {
                std::unique_lock<std::mutex> lk(mut);
                if (!running)
                    break;
                auto it = subs.find(subchannel);
                if (it != subs.end()) {
                    auto& sub = it->second;
                    zcm_recv_buf_t rbuf;
                    rbuf.data = buf;
                    rbuf.len = rc;
                    rbuf.utime = 0;
                    rbuf.zcm = this;
                    sub.cb(&rbuf, subchannel.c_str(), sub.usr);
                }
            }
        }
    }

    int subscribe(const string& channel, zcm_callback_t *cb, void *usr)
    {
        std::unique_lock<std::mutex> lk(mut);
        assert(subsocks.size() == 0);
        _getSubSock(channel);
        subs.emplace(channel, Sub{cb, usr});
        recvThread = std::thread{&zcm::recvThreadFunc, this};
        return 0;
    }
};

static void printBytes(const char *buf, size_t len)
{
    printf("printBytes:");
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        printf(" 0x%02x", c&0xff);
        if (isalpha(c))
            printf("(%c)", c);
    }
    printf("\n");
}

zcm_t *zcm_create(void)
{
    return new zcm_t();
}

void zcm_destroy(zcm_t *zcm)
{
    // TODO: What is the "correct" method to shut-down ZMQ?
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    return zcm->publish(channel, data, len);
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    return zcm->subscribe(channel, cb, usr);
}
