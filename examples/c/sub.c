#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <zcm/zcm.h>
#include "example_t.h"

static bool quiet = false;

static void my_handler(const zcm_recv_buf_t *rbuf, const char *channel,
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

int main(int argc, char *argv[])
{
    const char *CHANNEL = "EXAMPLE";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quiet") == 0)
            quiet = true;
        else
            CHANNEL = argv[i];
    }

    zcm_t *zcm = zcm_create("ipc");
    if(!zcm)
        return 1;

    example_t_subscribe(zcm, CHANNEL, &my_handler, NULL);

    while(1)
        zcm_handle(zcm);

    zcm_destroy(zcm);
    return 0;
}
