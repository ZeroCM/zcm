#include <iostream>
#include <cmath>
#include <sys/time.h>
#include <unistd.h>

#include <zcm/zcm-cpp.hpp>
#include <zcm/message_tracker.hpp>
#include <zcm/transport/generic_serial_transport.h>

#include "util/TimeUtil.hpp"

#include "types/example_t.hpp"

using namespace std;

atomic_int callbackTriggered {0};

constexpr double periodExpected = 1e3;

static void callback(example_t* msg, uint64_t utime, void* usr)
{
    callbackTriggered++;
    delete msg;
}
static constexpr uint32_t maxBufSize = 1e5;
static std::deque<uint8_t> buf;

uint32_t get(uint8_t* data, uint32_t nData, void* usr)
{
    uint32_t ret = std::min(nData, (uint32_t) buf.size());
    if (ret == 0) return 0;
    for (size_t i = 0; i < ret; ++i) { data[i] = buf.front(); buf.pop_front(); }
    return ret;
}

uint32_t put(const uint8_t* data, uint32_t nData, void* usr)
{
    uint32_t ret = std::min(nData, (uint32_t) (maxBufSize - buf.size()));
    if (ret == 0) return 0;
    for (size_t i = 0; i < ret; ++i) buf.push_back(data[i]);
    assert(buf.size() <= maxBufSize);
    return ret;
}

uint64_t timestamp_now(void* usr)
{
    static uint64_t i = 100;
    static uint64_t j = 0;
    static bool shouldIncrement = true;
    if (shouldIncrement) {
        j++;
        i += (periodExpected + sin(2 * M_PI * 100 * j * periodExpected / 1e6) *
             periodExpected / 10);
    }
    shouldIncrement = !shouldIncrement;
    return i;
}

int main(int argc, char *argv[])
{
    constexpr size_t numMsgs = 500;

    zcm_trans_t* trans = zcm_trans_generic_serial_create(get, put, NULL, timestamp_now, NULL);

    zcm::ZCM zcmLocal(trans);
    zcm::MessageTracker<example_t> mt(&zcmLocal, "EXAMPLE", numMsgs, numMsgs, callback);

    example_t msg = {0};

    uint64_t now[numMsgs];
    for (size_t i = 0; i < numMsgs; ++i) {
        now[i] = (uint64_t) (i * periodExpected);
        msg.utime = now[i];
        zcmLocal.publish("EXAMPLE", &msg);
        zcmLocal.flush();
    }

    usleep(1e5);
    assert(callbackTriggered > 0);

    example_t* recv = mt.get(now[3]);
    assert(recv);
    assert((uint64_t)recv->utime >= now[2] && (uint64_t)recv->utime <= now[4]);
    delete recv;

    //cout << mt.getHz() << endl;
    //cout << mt.getJitterUs() << endl;

    assert(mt.getHz() > 1e6 / periodExpected * 0.95 &&
           mt.getHz() < 1e6 / periodExpected * 1.05);

    assert(mt.getJitterUs() > 600 - 1e2 &&
           mt.getJitterUs() < 600 + 1e2);
}
