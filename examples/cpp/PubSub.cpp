#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include <string>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"
using namespace std;

static atomic_bool done {false};
static void sighandler(int s) { done = true; }

static const char *CHANNEL = "EXAMPLE";

class Handler
{
    public:
        ~Handler() {}

        void handleMessage(const zcm::ReceiveBuffer* rbuf,
                           const string& chan,
                           const example_t *msg)
        {
            printf("Received message on channel \"%s\":\n", chan.c_str());
            printf("  timestamp   = %lld\n", (long long)msg->timestamp);
            printf("  position    = (%f, %f, %f)\n",
                    msg->position[0], msg->position[1], msg->position[2]);
            printf("  orientation = (%f, %f, %f, %f)\n",
                    msg->orientation[0], msg->orientation[1],
                    msg->orientation[2], msg->orientation[3]);
            printf("  ranges:");
            for(int i = 0; i < msg->num_ranges; i++)
                printf(" %d", msg->ranges[i]);
            printf("\n");
            printf("  name        = '%s'\n", msg->name.c_str());
            printf("  enabled     = %d\n", msg->enabled);
        }
};

static void sendMessages(zcm::ZCM& zcm)
{
    example_t msg {};
    msg.timestamp = 0;

    msg.position[0] = 1;
    msg.position[1] = 2;
    msg.position[2] = 3;

    msg.orientation[0] = 1;
    msg.orientation[1] = 0;
    msg.orientation[2] = 0;
    msg.orientation[3] = 0;

    msg.num_ranges = 15;
    msg.ranges.resize(msg.num_ranges);
    for(int i = 0; i < msg.num_ranges; i++)
        msg.ranges[i] = i%20000;

    msg.name = "example string";
    msg.enabled = true;

    while (!done) {
        zcm.publish(CHANNEL, &msg);
        usleep(1000*1000);
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT,  sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    zcm::ZCM zcm {""};
    if (!zcm.good())
        return 1;

    Handler handler;
    zcm.subscribe(CHANNEL, &Handler::handleMessage, &handler);

    zcm.start();
    sendMessages(zcm);
    zcm.stop();
    zcm.writeTopology("zcm-example-pubsubcpp");

    return 0;
}
