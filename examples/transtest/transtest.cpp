#include <iostream>
#include <thread>
#include <cassert>
#include <cstdio>
#include <signal.h>
#include <unistd.h>
#include "zcm/transport_registrar.h"
using namespace std;

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

static void send(const char *url)
{
    auto *trans = makeTransport(url);
    assert(trans);

    zcm_msg_t msg;
    msg.channel = "FOO";
    msg.len = 100;
    msg.buf = (char*) malloc(msg.len);
    for (size_t i = 0; i < msg.len; i++)
        msg.buf[i] = (char)(i & 0xff);

    while (running_send) {
        zcm_trans_sendmsg(trans, msg);
        usleep(300000);
    }
}

static void recv(const char *url)
{
    auto *trans = makeTransport(url);
    assert(trans);

    // Tell the transport to give us all of the channels
    zcm_trans_recvmsg_enable(trans, NULL, true);

    while (running_recv) {
        zcm_msg_t msg;
        int ret = zcm_trans_recvmsg(trans, &msg, 100);
        if (ret == ZCM_EOK) {
            printf("recv: got message!\n");
            printf("  channel='%s'\n", msg.channel);
            printf("  len='%zu'\n", msg.len);
            printf("  buf=");
            for (size_t i = 0; i < msg.len; i++) {
                printf("%02x ", msg.buf[i] & 0xff);
            }
            printf("\n");
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
