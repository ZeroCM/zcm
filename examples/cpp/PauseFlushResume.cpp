#include <stdio.h>
#include <unistd.h>
#include <string>
#include <atomic>
#include <thread>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"
using namespace std;

static const uint16_t hz = 25;
static const char *CHANNEL = "EXAMPLE";

static atomic<bool> running {true};

class Handler
{
    public:
        ~Handler() {}

        void handleMessage(const zcm::ReceiveBuffer* rbuf,
                           const string& chan,
                           const example_t *msg)
        {
            printf("Received message on channel \"%s\":", chan.c_str());
            printf("  timestamp = %lld\n", (long long)msg->timestamp);
        }
};

void sendMessages()
{
    zcm::ZCM zcm {""};
    if (!zcm.good()) return;

    example_t msg {};
    msg.timestamp = 0;

    while (running) {
        zcm.publish(CHANNEL, &msg);
        ++msg.timestamp;
        usleep(1e6 / hz);
    }
}

int main(int argc, char *argv[])
{
    zcm::ZCM zcm {""};
    if (!zcm.good()) return 1;

    Handler handler;
    zcm.subscribe(CHANNEL, &Handler::handleMessage, &handler);

    zcm.setQueueSize(64);

    thread sendThread(&sendMessages);

    zcm.start();

    usleep(1e6 * 1);

    printf("\nPausing\n");
    zcm.pause();

    usleep(1e6 * 1);
    printf("\nFlushing\n");
    zcm.flush();

    usleep(1e6 * 1);

    printf("\nResuming\n");
    zcm.resume();

    usleep(1e6 * 1);

    printf("\nStopping\n");
    zcm.stop();

    running = false;
    sendThread.join();

    return 0;
}
