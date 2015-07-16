#include <unistd.h>
#include <zcm/zcm-cpp.hpp>
#include "example_t.hpp"

int main(int argc, char *argv[])
{
    const char *CHANNEL = "EXAMPLE";
    if (argc > 1)
        CHANNEL = argv[1];

    zcm::ZCM zcm;
    if (!zcm.good())
        return 1;

    example_t my_data;
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

    while (1) {
        zcm.publish(CHANNEL, &my_data);
        usleep(10);
    }

    return 0;
}
