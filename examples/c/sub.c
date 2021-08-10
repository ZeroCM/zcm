#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "types/example_t.h"

static bool quiet = false;

static int lastTimestamp = 0;

static void my_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                       const example_t *msg, void *user)
{
    if (lastTimestamp == 0 || msg->timestamp < lastTimestamp)
        lastTimestamp = msg->timestamp - 1;
    long diff = msg->timestamp - lastTimestamp;
    if (diff != 1) printf("Missed messages: %ld\n", diff);
    lastTimestamp = msg->timestamp;

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

int main(int argc, char *argv[])
{
    const char *channel = "EXAMPLE";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            zcm_transport_help(stdout);
            return 0;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else { // assume channel
            channel = argv[i];
        }
    }

    zcm_t *zcm = zcm_create("");
    if(!zcm)
        return 1;

    example_t_subscribe(zcm, channel, &my_handler, NULL);

    zcm_run(zcm);
    zcm_destroy(zcm);
    return 0;
}
