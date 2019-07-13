#ifndef TRACKERTEST_HPP
#define TRACKERTEST_HPP
#include <iostream>
#include <cmath>
#include <sys/time.h>
#include <unistd.h>
#include <random>

#include <zcm/zcm-cpp.hpp>
#include <zcm/message_tracker.hpp>
#include <zcm/transport/generic_serial_transport.h>

#include "util/TimeUtil.hpp"
#include "types/example_t.hpp"


#include "cxxtest/TestSuite.h"

using namespace std;

atomic_int callbackTriggered {0};

constexpr double periodExpected = 1e3;
constexpr double noiseStdDev = periodExpected / 30;

static void callback(example_t* msg, uint64_t utime, void* usr)
{
    callbackTriggered++;
    delete msg;
}
static constexpr uint32_t maxBufSize = 1e5;
static std::deque<uint8_t> buf;

static std::random_device rd;
static std::minstd_rand gen(rd());
static std::normal_distribution<double> dist(0.0, noiseStdDev);

size_t get(uint8_t* data, size_t nData, void* usr)
{
    uint32_t ret = std::min(nData, (size_t) buf.size());
    if (ret == 0) return 0;
    for (size_t i = 0; i < ret; ++i) { data[i] = buf.front(); buf.pop_front(); }
    return ret;
}

size_t put(const uint8_t* data, size_t nData, void* usr)
{
    uint32_t ret = std::min(nData, (size_t) (maxBufSize - buf.size()));
    if (ret == 0) return 0;
    for (size_t i = 0; i < ret; ++i) buf.push_back(data[i]);
    TS_ASSERT_LESS_THAN(buf.size(), maxBufSize);
    return ret;
}

uint64_t timestamp_now(void* usr)
{
    static uint64_t i = 100;
    static uint64_t j = 0;
    static bool shouldIncrement = true;
    if (shouldIncrement) {
        j++;
        i += periodExpected + dist(gen);
    }
    shouldIncrement = !shouldIncrement;
    return i;
}

class TrackerTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testTracker() {
        constexpr size_t numMsgs = 500;

        size_t mtu = 100;
        size_t bufSize = 1000;

        zcm_trans_t* trans = zcm_trans_generic_serial_create(get, put, NULL, timestamp_now, NULL, mtu, bufSize);
        TS_ASSERT(trans);
        zcm::ZCM zcmLocal(trans);
        zcm::MessageTracker<example_t> mt(&zcmLocal, "EXAMPLE", numMsgs, numMsgs, callback);


        example_t msg = {};

        size_t nTimesJitterWasRight = 0, nTimesHzWasRight = 0;

        uint64_t now[numMsgs];
        for (size_t i = 0; i < numMsgs; ++i) {
            now[i] = (uint64_t) (i * periodExpected);
            msg.utime = now[i];
            zcmLocal.publish("EXAMPLE", &msg);
            zcmLocal.flush();

            if (mt.getJitterUs() > noiseStdDev * 0.8 &&
                mt.getJitterUs() < noiseStdDev * 1.2) nTimesJitterWasRight++;

            if (mt.getHz() > 1e6 / periodExpected * 0.95 &&
                mt.getHz() < 1e6 / periodExpected * 1.05) nTimesHzWasRight++;
        }

        usleep(1e5);
        TS_ASSERT(callbackTriggered > 0);

        example_t* recv = mt.get(now[3]);
        TS_ASSERT(recv);
        TS_ASSERT((uint64_t)recv->utime >= now[2] && (uint64_t)recv->utime <= now[4]);
        delete recv;

        TS_ASSERT(nTimesJitterWasRight > 0.5 * numMsgs);
        TS_ASSERT(nTimesHzWasRight > 0.7 * numMsgs);
    }

};


#endif // TRACKERTEST_HPP
