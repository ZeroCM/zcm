#include <iostream>
#include <sys/time.h>
#include <unistd.h>

#include <zcm/zcm-cpp.hpp>
#include <zcm/message_tracker.hpp>
#include <zcm/transport/generic_serial_transport.h>

#include "util/TimeUtil.hpp"

#include "types/example_t.hpp"

using namespace std;

atomic_bool callbackTriggered {false};

static void callback(example_t* msg, uint64_t utime, void* usr)
{
    callbackTriggered = true;
    delete msg;
}
static constexpr uint32_t maxBufSize = 1e5;
static uint8_t *buf = new uint8_t[maxBufSize];
static uint32_t bufSize = 0;


uint32_t get(uint8_t* data, uint32_t nData, void* usr)
{
    uint32_t ret = std::min(nData, bufSize);
    if (ret == 0) return 0;
    memcpy(data, buf, ret);
    bufSize -= ret;
    return ret;
}

uint32_t put(const uint8_t* data, uint32_t nData, void* usr)
{
    uint32_t ret = std::min(nData, maxBufSize - bufSize);
    if (ret == 0) return 0;
    memcpy(buf + bufSize, data, ret);
    bufSize += ret;
    assert(bufSize <= maxBufSize);
    return ret;
}

uint64_t timestamp_now(void* usr)
{
    static uint64_t i = 0;
    return i++;
}

int main(int argc, char *argv[])
{
    constexpr size_t numMsgs = 5;

    zcm_trans_t* trans = zcm_trans_generic_serial_create(get, put, NULL, timestamp_now, NULL);

    zcm::ZCM zcmLocal(trans);
    zcm::MessageTracker<example_t> mt(&zcmLocal, "EXAMPLE", 0.25, numMsgs, callback);

    example_t msg = {0};

    uint64_t now[numMsgs];
    for (size_t i = 0; i < numMsgs; ++i) {
        now[i] = i;
        msg.utime = i;
        zcmLocal.publish("EXAMPLE", &msg);
    }

    zcmLocal.flush();

    assert(callbackTriggered);
    (void) now;
    example_t* recv = mt.get(now[3]);
    assert(recv);
    //cout << recv->utime << endl;
    assert(recv->utime == 3);
    delete recv;
    delete[] buf;
}
