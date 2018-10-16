#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "types/example_t.h"

#define HZ 10
#define NRANGES 100
#define CHANNEL "EXAMPLE"

static bool quiet = false;

static void handler(const zcm_recv_buf_t *rbuf, const char *channel,
                    const example_t *msg, void *user)
{
    if (quiet)
        return;

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

static void send_msgs(zcm_t *zcm)
{
    example_t msg = {};

    msg.timestamp = 0,
    msg.position[0] = 1;
    msg.position[1] = 2;
    msg.position[2] = 3;
    msg.orientation[0] = 1;
    msg.orientation[1] = 0;
    msg.orientation[2] = 0;
    msg.orientation[3] = 0;

    msg.num_ranges = NRANGES;
    msg.ranges = calloc(msg.num_ranges, sizeof(int16_t));
    msg.name = "example string";
    msg.enabled = 1;

    while (1) {
        example_t_publish(zcm, CHANNEL, &msg);
        usleep(1e6/HZ);
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            zcm_transport_help(stdout);
            return 0;
        }
    }

    zcm_t *zcm = zcm_create("");
    if (!zcm)
        return 1;

    example_t_subscribe(zcm, CHANNEL, &handler, NULL);
    zcm_start(zcm);

    send_msgs(zcm);

    zcm_stop(zcm);
    zcm_destroy(zcm);
    return 0;
}
