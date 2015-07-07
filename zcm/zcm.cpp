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

#define PUB_ADDR "ipc:///tmp/pub"
#define SUB_ADDR "ipc:///tmp/sub"
#define ZMQ_IO_THREADS 1

struct Sub
{
    zcm_callback_t *cb;
    void *usr;
};

struct zcm
{
    void *ctx;
    void *pubsock;
    void *subsock;

    std::thread recvThread;

    // The mutex protects all data below it
    std::mutex mut;
    bool running = true;
    unordered_map<string, Sub> subs;
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

static void recvThreadFunc(zcm_t *zcm)
{
    const int BUFSZ = 1 << 20;
    char *buf = (char *) malloc(BUFSZ);
    int rc;

    while (1) {
        rc = zmq_recv(zcm->subsock, buf, BUFSZ, 0);
        assert(0 < rc && rc < BUFSZ);
        string channel = string(buf, rc);
        //printBytes(buf, rc);
        rc = zmq_recv(zcm->subsock, buf, BUFSZ, 0);
        assert(0 < rc && rc < BUFSZ);

        // dispatch
        {
            std::unique_lock<std::mutex> lk(zcm->mut);
            if (!zcm->running)
                break;
            auto it = zcm->subs.find(channel);
            if (it != zcm->subs.end()) {
                auto& sub = it->second;
                sub.cb(zcm, channel.c_str(), buf, rc, sub.usr);
            }
        }
    }
}

zcm_t *zcm_create(void)
{
    zcm_t *zcm = new zcm_t();
    zcm->ctx = zmq_init(ZMQ_IO_THREADS);
    zcm->pubsock = zmq_socket(zcm->ctx, ZMQ_PUB);
    zcm->subsock = zmq_socket(zcm->ctx, ZMQ_SUB);

    // XXX need to do error checking!
    zmq_connect(zcm->pubsock, PUB_ADDR);
    zmq_connect(zcm->subsock, SUB_ADDR);
    zmq_setsockopt(zcm->subsock, ZMQ_SUBSCRIBE, "", 0);

    zcm->recvThread = std::thread{recvThreadFunc, zcm};

    return zcm;
}

void zcm_destroy(zcm_t *zcm)
{
    // TODO: What is the "correct" method to shut-down ZMQ?
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    int channel_len = strlen(channel);
    int rc;

    rc = zmq_send(zcm->pubsock, channel, channel_len, ZMQ_SNDMORE);
    assert(rc == channel_len);
    rc = zmq_send(zcm->pubsock, data, len, 0);
    assert(rc == (int)len);

    return 0;
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    // TODO: Integrate directly with ZMQ to use it's subscription filtering
    //       technique in setsockopt. This isn't trivial to do because we need to
    //       execute the operation from the recv thread to ensure thread safety
    {
        std::unique_lock<std::mutex> lk(zcm->mut);
        zcm->subs.emplace(channel, Sub{cb, usr});
    }
    return 0;
}
