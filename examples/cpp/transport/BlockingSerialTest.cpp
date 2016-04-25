#include <iostream>
#include <cassert>
#include <sys/time.h>

#include "zcm/zcm-cpp.hpp"

#include "types/example_t.hpp"

using namespace std;
using namespace zcm;

#define PUBLISH_DT (1e6)/(500)

static uint64_t utime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

class Handler
{
    int64_t lastHost = 0;

  public:
    void handle(const ReceiveBuffer* rbuf, const string& chan, const example_t* msg)
    {
        cout << "Got one: " << msg->timestamp << endl;

        if (msg->timestamp <= lastHost)  {
            cout << "ERROR" << endl;
            assert(false && "Got negative dt");
        }
        lastHost = msg->timestamp;

    }
};

int main(int argc, const char *argv[])
{
    ZCM zcmLocal("serial:///dev/ftdi-cable?baud=115200");

    example_t example;
    example.num_ranges = 1;
    example.ranges.resize(1);
    example.ranges.at(0) = 1;

    Handler handler;
    auto sub = zcmLocal.subscribe("EXAMPLE", &Handler::handle, &handler);

    zcmLocal.start();

    uint64_t nextPublish = utime();
    while (true)
    {
        uint64_t now = utime();
        if (now > nextPublish) {
            cout << "Publishing" << endl;
            example.timestamp = utime();
            zcmLocal.publish("EXAMPLE", &example);
            nextPublish = now + PUBLISH_DT;
        }

    }

    zcmLocal.stop();

    zcmLocal.unsubscribe(sub);

    return 0;
}
