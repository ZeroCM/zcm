#include "zcm.h"
#include <zmq.h>

#include <unistd.h>

#include <cstring>
#include <cassert>
#include <cctype>
#include <cstdio>

#include <vector>
using std::vector;

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
    bool recvThreadStarted = false;
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

    void recvMsgFromSock(const string& channel, void *sock)
    {
        const int BUFSZ = 1 << 20;
        char *buf = (char *) malloc(BUFSZ);

        int rc = zmq_recv(sock, buf, BUFSZ, 0);
        assert(0 < rc && rc < BUFSZ);

        // Dispatch
        {
            std::unique_lock<std::mutex> lk(mut);
            auto it = subs.find(channel);
            if (it != subs.end()) {
                auto& sub = it->second;
                zcm_recv_buf_t rbuf;
                rbuf.data = buf;
                rbuf.len = rc;
                rbuf.utime = 0;
                rbuf.zcm = this;
                sub.cb(&rbuf, channel.c_str(), sub.usr);
            }
        }
    }

    void recvThreadFunc()
    {
        while (1) {
            // Build up a list of poll items
            vector<zmq_pollitem_t> pitems;
            vector<string> pchannels;
            {
                std::unique_lock<std::mutex> lk(mut);
                pitems.resize(subsocks.size());
                int i = 0;
                for (auto& elt : subsocks) {
                    auto& channel = elt.first;
                    auto& sock = elt.second;
                    auto *p = &pitems[i];
                    memset(p, 0, sizeof(*p));
                    p->socket = sock;
                    p->events = ZMQ_POLLIN;
                    pchannels.emplace_back(channel);
                    i++;
                }
            }

            int rc = zmq_poll(pitems.data(), pitems.size(), 100);
            if (!running)
                break;
            if (rc >= 0) {
                for (size_t i = 0; i < pitems.size(); i++) {
                    auto& p = pitems[i];
                    if (p.revents != 0)
                        recvMsgFromSock(pchannels[i], p.socket);
                }
            }
        }
    }

    int subscribe(const string& channel, zcm_callback_t *cb, void *usr)
    {
        std::unique_lock<std::mutex> lk(mut);
        _getSubSock(channel);
        subs.emplace(channel, Sub{cb, usr});
        if (!recvThreadStarted) {
            recvThread = std::thread{&zcm::recvThreadFunc, this};
            recvThreadStarted = true;
        }
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
