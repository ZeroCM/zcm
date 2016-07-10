#include <iostream>
#include <thread>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include "zcm/transport_registrar.h"
#include "util/TimeUtil.hpp"
using namespace std;

#define HZ 500
#define MSG_COUNT 100

static bool BIG_MESSAGE = true;
volatile bool running_recv = true;
volatile bool running_send = true;
static void sighandler(int sig) { running_recv = false; }

static zcm_trans_t *makeTransport()
{
    const char *url = getenv("ZCM_DEFAULT_URL");
    if (!url) {
        fprintf(stderr, "ERR: Unable to find environment variable ZCM_DEFAULT_URL\n");
        return NULL;
    }
    auto *u = zcm_url_create(url);
    auto *creator = zcm_transport_find(zcm_url_protocol(u));
    if (!creator) {
        fprintf(stderr, "ERR: Failed to create transport for '%s'\n", url);
        return NULL;
    }
    return creator(u);
}

static zcm_msg_t makeMasterMsg()
{
    zcm_msg_t msg;
    msg.utime = TimeUtil::utime();
    msg.channel = "FOO";
    msg.len = BIG_MESSAGE ? 500000 : 1000;
    msg.buf = (char*) malloc(msg.len);
    for (size_t i = 0; i < msg.len; i++)
        msg.buf[i] = (char)(i & 0xff);

    return msg;
}

#define fail(...) \
    do {\
        fprintf(stderr, "Err:"); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        exit(1); \
    } while(0)

static void verifySame(zcm_msg_t *a, zcm_msg_t *b)
{
    if (b->utime < a->utime || a->utime == 0 || b->utime == 0)
        fail("Utime failure!");
    if (strcmp(a->channel, b->channel) != 0)
        fail("Channels don't match!");
    if (a->len != b->len)
        fail("Lengths don't match!");
    for (size_t i = 0; i < a->len; i++)
        if (a->buf[i] != b->buf[i])
            fail("Data doesn't match at index %d", (int)i);
}

static void send()
{
    auto *trans = makeTransport();
    if (!trans)
        exit(1);

    usleep(10000); // sleep 10ms so the recv thread can come up

    zcm_msg_t msg = makeMasterMsg();
    for (int i = 0; i < MSG_COUNT && running_send; i++) {
        zcm_trans_sendmsg(trans, msg);
        usleep(1000000 / HZ);
    }
}

static void recv()
{
    auto *trans = makeTransport();
    if (!trans)
        exit(1);

    // Tell the transport to give us all of the channels
    zcm_trans_recvmsg_enable(trans, NULL, true);

    zcm_msg_t master = makeMasterMsg();
    uint64_t start = TimeUtil::utime();
    int i;
    for (i = 0; i < MSG_COUNT && running_recv; i++) {
        zcm_msg_t msg;
        int ret = zcm_trans_recvmsg(trans, &msg, 100);
        if (ret == ZCM_EOK) {
            verifySame(&master, &msg);
        }
    }
    uint64_t end = TimeUtil::utime();

    cout << "Received " << (i * 100 / MSG_COUNT) << "\% of the messages in "
         << ((end - start) / 1e6) << " seconds" <<  endl;

    running_send = false;
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        string opt {argv[1]};
        if (opt == "--small")
            BIG_MESSAGE = false;
    }

    signal(SIGINT, sighandler);

    std::thread sendThread {send};
    std::thread recvThread {recv};

    recvThread.join();
    sendThread.join();

    return 0;
}
