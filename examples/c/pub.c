#include <stdio.h>
#include <unistd.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "types/example_t.h"

static int HZ = 10;
//#define NRANGES 1000000
#define NRANGES 100

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            zcm_transport_help(stdout);
            return 0;
        } else { // assume Hz
            HZ = atoi(argv[i]);
        }
    }

    zcm_t *zcm = zcm_create("");
    if (!zcm)
        return 1;

    example_t my_data = {};

    my_data.timestamp = 0,
    my_data.position[0] = 1;
    my_data.position[1] = 2;
    my_data.position[2] = 3;
    my_data.orientation[0] = 1;
    my_data.orientation[1] = 0;
    my_data.orientation[2] = 0;
    my_data.orientation[3] = 0;

    my_data.num_ranges = NRANGES;
    my_data.ranges = calloc(my_data.num_ranges, sizeof(int16_t));
    my_data.name = EXAMPLE_T_test_const_string;
    my_data.enabled = 1;

    uint64_t sleepUs = 1000000/HZ;
    while (1) {
        if (example_t_publish(zcm, "EXAMPLE", &my_data) == 0)
            ++my_data.timestamp;
        if (sleepUs > 0) usleep(sleepUs);
    }

    zcm_destroy(zcm);
    return 0;
}
