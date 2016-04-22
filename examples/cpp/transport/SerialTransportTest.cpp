#include <iostream>
#include <queue>
#include <sys/time.h>

#include "zcm/zcm-cpp.hpp"

#include "generic_serial_transport.h"
#include "types/example_t.hpp"

using namespace std;
using namespace zcm;

#define PUBLISH_DT 25e4
#define BUFFER_SIZE 200
#define MIN(A, B) ((A) < (B)) ? (A) : (B)

queue<uint8_t> fifo;

static uint32_t get(uint8_t* data, uint32_t nData)
{
    unsigned i;
    for (i = 0; i < nData; i++)
        if (!fifo.empty()) {
            data[i] = fifo.front();
            fifo.pop();
        }

    return i;
}

static uint32_t put(const uint8_t* data, uint32_t nData)
{
    unsigned i;
    for (i = 0; i < nData; i++)
        fifo.push(data[i]);

    return nData;
}

static uint64_t utime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

class Handler
{
  public:
    void handle(const ReceiveBuffer* rbuf, const string& chan, const example_t* msg)
    {
        cout << "Got one" << endl;
    }
};

int main(int argc, const char *argv[])
{
    ZCM zcmLocal(zcm_trans_generic_serial_create(&get, &put));

    example_t example;
    example.num_ranges = 1;
    example.ranges.resize(1);
    example.ranges.at(0) = 1;

    Handler handler;
    auto sub = zcmLocal.subscribe("EXAMPLE", &Handler::handle, &handler);

    uint64_t nextPublish = utime();
    while (true)
    {
        uint64_t now = utime();
        if (now > nextPublish) {
            cout << "Publishing" << endl;
            zcmLocal.publish("EXAMPLE", &example);
            nextPublish = now + PUBLISH_DT;
        }

        zcm_handle_nonblock(zcmLocal.getUnderlyingZCM());
    }

    zcmLocal.unsubscribe(sub);

    return 0;
}
