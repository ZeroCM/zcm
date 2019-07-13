#ifndef FLUSHINGTEST_HPP
#define FLUSHINGTEST_HPP

#include <unistd.h>
#include <thread>

#include <zcm/zcm.h>
#include "cxxtest/TestSuite.h"

#define CHANNEL "TEST_CHANNEL"
#define N 100

static volatile size_t numrecv = 0;
static volatile bool subrunning = false;

static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    numrecv++;
}

void subscriber_thread_function()
{
    zcm_t *zcm = zcm_create("udpm://239.255.76.67:7667?ttl=0");
    zcm_subscribe(zcm, CHANNEL, handler, NULL);

    zcm_start(zcm);
    subrunning = true;
    while (subrunning) {
        usleep(100000);
    }
    zcm_stop(zcm);

    zcm_destroy(zcm);
}


class FlushingTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testFlushing() {
        // start sub
        std::thread subthread {subscriber_thread_function};

        // busy wait for subthread
        while (!subrunning) {}

        // do pub
        uint8_t data = 'A';
        for (size_t i = 0; i < N; i++) {
            // create, send, and destroy
            zcm_t *zcm = zcm_create("udpm://239.255.76.67:7667?ttl=0");
            assert(zcm);
            zcm_publish(zcm, CHANNEL, &data, 1);
            zcm_flush(zcm);
            zcm_destroy(zcm);
        }


        // shutdown
        subrunning = false;
        subthread.join();

        TS_ASSERT_EQUALS(numrecv, N);
    }
};

#endif // FLUSHINGTEST_HPP
