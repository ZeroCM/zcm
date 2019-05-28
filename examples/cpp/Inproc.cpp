#include <stdio.h>
#include <unistd.h>
#include <string>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"
using std::string;

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

int main(int argc, char *argv[])
{
    const char *CHANNEL = "EXAMPLE";

    zcm::ZCM zcm {"inproc"};
    if (!zcm.good())
        return 1;

    Handler handlerObject;
    auto subs = zcm.subscribe(".*", &Handler::handleMessage, &handlerObject);

    example_t my_data {};
    my_data.timestamp = 0;

    my_data.position[0] = 1;
    my_data.position[1] = 2;
    my_data.position[2] = 3;

    my_data.orientation[0] = 1;
    my_data.orientation[1] = 0;
    my_data.orientation[2] = 0;
    my_data.orientation[3] = 0;

    my_data.num_ranges = 15;
    my_data.ranges.resize(my_data.num_ranges);
    for(int i = 0; i < my_data.num_ranges; i++)
        my_data.ranges[i] = i;

    my_data.name = "example string";
    my_data.enabled = true;

    zcm.start();

    while (1) {
        zcm.publish(CHANNEL, &my_data);
        zcm.publish("FOOBAR", &my_data);
        usleep(1e6);
    }

    zcm.stop();

    zcm.unsubscribe(subs);

    return 0;
}
