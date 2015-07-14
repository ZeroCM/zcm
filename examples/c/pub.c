#include <stdio.h>
#include <unistd.h>
#include <zcm/zcm.h>
#include "example_t.h"

int main(void)
{
    zcm_t *zcm = zcm_create();
    if (!zcm)
        return 1;

    example_t my_data = {
        .timestamp = 0,
        .position = { 1, 2, 3 },
        .orientation = { 1, 0, 0, 0 },
    };

    my_data.num_ranges = 10000;
    my_data.ranges = calloc(my_data.num_ranges, sizeof(int16_t));
    my_data.name = "example string";
    my_data.enabled = 1;

    while (1) {
        example_t_publish(zcm, "EXAMPLE", &my_data);
        usleep(10);
    }

    zcm_destroy(zcm);
    return 0;
}
