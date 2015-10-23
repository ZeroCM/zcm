#include <stdio.h>
#include <unistd.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "types/example_t.h"

#define HZ 10
//#define NRANGES 10000
#define NRANGES 100

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            zcm_transport_help(stdout);
            return 0;
        }
    }

    zcm_t *zcm = zcm_create("ipc");
    if (!zcm)
        return 1;

    example_t my_data = {
        .timestamp = 0,
        .position = { 1, 2, 3 },
        .orientation = { 1, 0, 0, 0 },
    };

    my_data.num_ranges = NRANGES;
    my_data.ranges = calloc(my_data.num_ranges, sizeof(int16_t));
    my_data.name = "example string";
    my_data.enabled = 1;

    while (1) {
        example_t_publish(zcm, "EXAMPLE", &my_data);
        usleep(1000000/HZ);
    }

    zcm_destroy(zcm);
    return 0;
}
