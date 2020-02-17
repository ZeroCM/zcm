#include <unistd.h>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"

int main(int argc, char *argv[])
{
    zcm::ZCM zcm {""};
    if (!zcm.good())
        return 1;

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
    my_data.num_ranges = 15;
    my_data.ranges.resize(my_data.num_ranges);
    for(int i = 0; i < my_data.num_ranges; i++)
        my_data.ranges[i] = i%20000;

    my_data.name = "example string";
    my_data.enabled = true;

    while (1) {
        zcm.publish("EXAMPLE", &my_data);
        for (auto& val : my_data.position) val++;
        usleep(1000*1000);
    }

    return 0;
}
