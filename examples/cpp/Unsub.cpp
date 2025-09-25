#include <stdio.h>
#include <unistd.h>
#include <string>
#include <zcm/zcm-cpp.hpp>
#include <sys/time.h>
#include "types/example_t.hpp"
using namespace std;

static const char *CHANNEL = "EXAMPLE";

static uint64_t utime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

class Handler
{
  public:
    size_t id;
    size_t msgsReceived = 0;

    Handler(size_t id) : id(id) {}
    ~Handler() {}

    void handleMessage(const zcm::ReceiveBuffer* rbuf,
                       const string& chan,
                       const example_t *msg)
    {
        ++msgsReceived;
    }
};

static void dispatchOneMessage(zcm::ZCM& zcm, uint64_t sleepTimeUs)
{
    if (zcm.getUnderlyingZCM()->type == ZCM_BLOCKING) {
        usleep(sleepTimeUs);
    } else {
        uint64_t end = utime() + sleepTimeUs;
        while (utime() < end) {
            if (zcm.handle(0) == ZCM_EOK) break;
        }
    }
}


int main(int argc, char *argv[])
{
    zcm::ZCM zcm;
    if (!zcm.good())
        return 1;

    example_t msg {};

    Handler h1(1);
    Handler h2(2);
    Handler h3(3);
    Handler h4(4);

    auto* sub1 = zcm.subscribe(CHANNEL, &Handler::handleMessage, &h1);
    auto* sub2 = zcm.subscribe(CHANNEL, &Handler::handleMessage, &h2);
    auto* sub3 = zcm.subscribe(".*", &Handler::handleMessage, &h3);
    auto* sub4 = zcm.subscribe(".*", &Handler::handleMessage, &h4);

    if (zcm.getUnderlyingZCM()->type == ZCM_BLOCKING) zcm.start();

    zcm.publish(CHANNEL, &msg);
    dispatchOneMessage(zcm, 1000*1000);
    zcm.flush();

    h1.msgsReceived = 0;
    h2.msgsReceived = 0;
    h3.msgsReceived = 0;
    h4.msgsReceived = 0;

    for (size_t i = 0; i < 5; ++i) {
        zcm.publish(CHANNEL, &msg);
        dispatchOneMessage(zcm, 100000);
    }

    zcm.flush();
    zcm.unsubscribe(sub1);

    for (size_t i = 0; i < 5; ++i) {
        zcm.publish(CHANNEL, &msg);
        dispatchOneMessage(zcm, 100000);
    }

    zcm.flush();
    zcm.unsubscribe(sub2);

    for (size_t i = 0; i < 5; ++i) {
        zcm.publish(CHANNEL, &msg);
        dispatchOneMessage(zcm, 100000);
    }

    zcm.flush();
    zcm.unsubscribe(sub3);

    for (size_t i = 0; i < 5; ++i) {
        zcm.publish(CHANNEL, &msg);
        dispatchOneMessage(zcm, 100000);
    }

    zcm.flush();
    zcm.unsubscribe(sub4);

    if (zcm.getUnderlyingZCM()->type == ZCM_BLOCKING) zcm.stop();

    size_t ret = 0;

    auto verify = [&ret](const Handler& h, size_t nExp) {
        if (h.msgsReceived != nExp) {
            printf("Received wrong number of messages on handler %zu: %zu\n",
                   h.id, h.msgsReceived);
            ret = 1;
        }
    };

    verify(h1,  5);
    verify(h2, 10);
    verify(h3, 15);
    verify(h4, 20);

    if (ret == 0) printf("Success\n");
    return ret;
}
