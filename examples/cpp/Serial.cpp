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
    const char *URL = "serial:///dev/ttyACM0?baud=115200";
    if (argc > 1) URL = argv[1];

    zcm::ZCM zcm {URL};
    if (!zcm.good())
        return 1;

    Handler handlerObject;
    auto subs = zcm.subscribe("EXAMPLE", &Handler::handleMessage, &handlerObject);

    example_t my_data {};
    my_data.timestamp = 0;

    my_data.position[0] = 1;
    my_data.position[1] = 2;
    my_data.position[2] = 3;

    my_data.orientation[0] = 1;
    my_data.orientation[1] = 0;
    my_data.orientation[2] = 0;
    my_data.orientation[3] = 0;

    //my_data.num_ranges = 100000;
    my_data.num_ranges = 1;
    my_data.ranges.resize(my_data.num_ranges);
    for(int i = 0; i < my_data.num_ranges; i++)
        my_data.ranges[i] = i%20000;

    my_data.name = "e";
    my_data.enabled = true;

    zcm.start();

    while (1) {
        zcm.publish("EXAMPLE", &my_data);
        usleep(1000*1000);
    }

    zcm.stop();

    zcm.unsubscribe(subs);

    return 0;
}
