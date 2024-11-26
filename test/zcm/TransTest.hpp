#ifndef TRANSTEST_H
#define TRANSTEST_H

#include <thread>
#include <unistd.h>
#include <signal.h>


#include "cxxtest/TestSuite.h"
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
    TSM_ASSERT("Unable to find environment variable ZCM_DEFAULT_URL", url);
    auto *u = zcm_url_create(url);
    auto *creator = zcm_transport_find(zcm_url_protocol(u));
    TSM_ASSERT("Failed to create transport", creator);
    zcm_trans_t* ret = creator(u, NULL);
    zcm_url_destroy(u);
    return ret;
}

static zcm_msg_t makeMasterMsg()
{
    zcm_msg_t msg;
    msg.utime = TimeUtil::utime();
    msg.channel = "FOO";
    msg.len = BIG_MESSAGE ? 500000 : 1000;
    msg.buf = (uint8_t*) malloc(msg.len);
    for (size_t i = 0; i < msg.len; i++)
        msg.buf[i] = (uint8_t)(i & 0xff);

    return msg;
}

static void deleteMasterMsg(zcm_msg_t* msg)
{
    free(msg->buf);
}

static void verifySame(zcm_msg_t *a, zcm_msg_t *b)
{
    if (b->utime < a->utime || a->utime == 0 || b->utime == 0)
        TS_FAIL("Ultimate failure!");
    if (strcmp(a->channel, b->channel) != 0)
        TS_FAIL("Channels don't match!");
    if (a->len != b->len)
        TS_FAIL("Lengths don't match!");
    for (size_t i = 0; i < a->len; i++)
        if (a->buf[i] != b->buf[i])
            TS_FAIL("Data doesn't match");
}

static void send()
{
    auto *trans = makeTransport();
    TSM_ASSERT("Failed to create transport", trans);
    if (!trans)
        exit(1);

    usleep(10000); // sleep 10ms so the recv thread can come up

    zcm_msg_t msg = makeMasterMsg();
    // For transports that require a "warm up" message
    zcm_trans_sendmsg(trans, msg);
    usleep(1e6);

    for (int i = 0; i < 3 * MSG_COUNT && running_send; i++) {
        zcm_trans_sendmsg(trans, msg);
        usleep(1000000 / HZ);
    }
    deleteMasterMsg(&msg);
    zcm_trans_destroy(trans);
}

static void recv()
{
    auto *trans = makeTransport();
    TSM_ASSERT("Failed to create transport", trans);
    if (!trans)
        exit(1);

    // Tell the transport to give us all of the channels
    zcm_trans_recvmsg_enable(trans, ".*", true);

    int i;
    zcm_msg_t master = makeMasterMsg();

//    uint64_t start = TimeUtil::utime();

    for (i = 0; i < MSG_COUNT && running_recv;) {
        zcm_msg_t msg;
        int ret = zcm_trans_recvmsg(trans, &msg, 100);
        if (ret == ZCM_EOK) {
            verifySame(&master, &msg);
            ++i;
        }
    }

    zcm_trans_recvmsg_enable(trans, master.channel, true);
    zcm_trans_recvmsg_enable(trans, master.channel, false);

    for (i = 0; i < MSG_COUNT && running_recv;) {
        zcm_msg_t msg;
        int ret = zcm_trans_recvmsg(trans, &msg, 100);
        if (ret == ZCM_EOK) {
            verifySame(&master, &msg);
            ++i;
        }
    }


//    uint64_t end = TimeUtil::utime();

//    cout << "Received " << (i * 100 / MSG_COUNT) << "\% of the messages in "
//         << ((end - start) / 1e6) << " seconds" <<  endl;

    running_send = false;
    deleteMasterMsg(&master);
    zcm_trans_destroy(trans);
}

class TransTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testTransport() {
        signal(SIGINT, sighandler);

        std::thread sendThread {send};
        std::thread recvThread {recv};

        recvThread.join();
        sendThread.join();
    }
};


#endif // TRANSTEST_H
