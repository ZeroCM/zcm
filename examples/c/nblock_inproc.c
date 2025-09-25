#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "types/example_t.h"

static void my_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                       const example_t *msg, void *user)
{
    /* /\* if (quiet) */
    /*     return; */

    printf("Received message on channel \"%s\":\n", channel);
    printf("  timestamp   = %"PRId64"\n", msg->timestamp);
    printf("  position    = (%f, %f, %f)\n",
            msg->position[0], msg->position[1], msg->position[2]);
    printf("  orientation = (%f, %f, %f, %f)\n",
            msg->orientation[0], msg->orientation[1], msg->orientation[2],
            msg->orientation[3]);
    printf("  ranges:");
    for(int i = 0; i < msg->num_ranges; i++)
        printf(" %d", msg->ranges[i]);
    printf("\n");
    printf("  name        = '%s'\n", msg->name);
    printf("  enabled     = %d\n", msg->enabled);
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            zcm_transport_help(stdout);
            return 0;
        }
    }

    zcm_t zcm;
    if (zcm_init(&zcm, "nonblock-inproc", NULL) != 0) return 1;

    example_t_subscription_t* exSub = example_t_subscribe(&zcm, "EXAMPLE", &my_handler, NULL);

    example_t my_data = {};

    my_data.timestamp = 0,
    my_data.position[0] = 1;
    my_data.position[1] = 2;
    my_data.position[2] = 3;
    my_data.orientation[0] = 1;
    my_data.orientation[1] = 0;
    my_data.orientation[2] = 0;
    my_data.orientation[3] = 0;

    my_data.num_ranges = 15;
    my_data.ranges = calloc(my_data.num_ranges, sizeof(int16_t));
    my_data.name = "example string";
    my_data.enabled = 1;

    size_t i;
    for (i = 0; i < 10; ++i) {
        my_data.timestamp = i;
        example_t_publish(&zcm, "EXAMPLE", &my_data);
        usleep(100000);
        zcm_handle(&zcm, 0);
    }

    free(my_data.ranges);

    example_t_unsubscribe(&zcm, exSub);

    zcm_cleanup(&zcm);
    return 0;
}
