#include <stdio.h>
#include <unistd.h>
#include <string>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"
using std::string;

int main(int argc, char *argv[])
{
    const char *URL = "ipc";
    if (argc > 1) URL = argv[1];

    zcm::ZCM zcm {URL};
    if (!zcm.good())
        return 1;

    zcm.subscribe<example_t>("EXAMPLE", [](const zcm::ReceiveBuffer* rbuf,
                                           const string& chan,
                                           const example_t *msg){
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
        });
    zcm.run();

    return 0;
}
