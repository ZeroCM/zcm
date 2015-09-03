#include <iostream>
#include <thread>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include "zcm/transport_registrar.h"
using namespace std;

#define HZ 10

volatile bool running_recv = true;
volatile bool running_send = true;
static void sighandler(int sig) { running_recv = false; }

static zcm_trans_t *makeTransport(const char *url)
{
    auto *u = zcm_url_create(url);
    auto *creator = zcm_transport_find(zcm_url_protocol(u));
    if (!creator)
        return NULL;
    return creator(u);
}

static zcm_msg_t makeMasterMsg()
{
    zcm_msg_t msg;
    msg.channel = "FOO";
    msg.len = 70000;
    msg.buf = (char*) malloc(msg.len);
    for (size_t i = 0; i < msg.len; i++)
        msg.buf[i] = (char)(i & 0xff);

    return msg;
}

#define fail(...) do {\
    fprintf(stderr, "Err:"); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1);\
    } while(0)

static void verifySame(zcm_msg_t *a, zcm_msg_t *b)
{
    if (strcmp(a->channel, b->channel) != 0)
        fail("Channels don't match!");
    if (a->len != b->len)
        fail("Lengths don't match!");
    for (size_t i = 0; i < a->len; i++)
        if (a->buf[i] != b->buf[i])
            fail("Data doesn't match at index %d", (int)i);
}

static void send(const char *url)
{
    auto *trans = makeTransport(url);
    if (!trans) {
        fprintf(stderr, "ERR: Failed to create transport for '%s'\n", url);
        exit(1);
    }

    zcm_msg_t msg = makeMasterMsg();
    while (running_send) {
        zcm_trans_sendmsg(trans, msg);
        usleep(1000000/HZ);
    }
}

static void recv(const char *url)
{
    auto *trans = makeTransport(url);
    if (!trans) {
        fprintf(stderr, "ERR: Failed to create transport for '%s'\n", url);
        exit(1);
    }

    // Tell the transport to give us all of the channels
    zcm_trans_recvmsg_enable(trans, NULL, true);

    zcm_msg_t master = makeMasterMsg();
    while (running_recv) {
        zcm_msg_t msg;
        int ret = zcm_trans_recvmsg(trans, &msg, 100);
        if (ret == ZCM_EOK) {
            verifySame(&master, &msg);
            printf("SUCCESS\n");
        }
    }

    // XXX this is a hack for when the recvmsg timeout is not impl correctly
    running_send = false;
}

int main()
{
    signal(SIGINT, sighandler);

    std::thread sendThread {send, "udpm://239.255.76.67?port=7667&ttl=0"};
    std::thread recvThread {recv, "udpm://239.255.76.67?port=7667&ttl=0"};

    recvThread.join();
    sendThread.join();

    return 0;
}
