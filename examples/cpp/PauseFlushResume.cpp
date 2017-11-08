#include <stdio.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"
using namespace std;

static const uint16_t hz = 1;
static const char *CHANNEL = "EXAMPLE";

static bool running = true;

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

void sendMessages(zcm::ZCM& zcm)
{
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
    if (!zcm.good())
        return 1;

    Handler handler;
    zcm.subscribe(CHANNEL, &Handler::handleMessage, &handler);

    thread sendThread(&sendMessages, ref(zcm));

    zcm.start();

    usleep(1e6 * 5);

    zcm.pause();

    usleep(1e6 * 5);

    zcm.resume();

    usleep(1e6 * 1);

    zcm.stop();

    running = false;
    sendThread.join();

    return 0;
}
