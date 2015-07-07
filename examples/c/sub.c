#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <zcm/zcm.h>
#include "example_t.h"

static void my_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                       const example_t *msg, void *user)
{
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
    zcm_t *zcm = zcm_create();
    if(!zcm)
        return 1;

    example_t_subscribe(zcm, "EXAMPLE", &my_handler, NULL);

    while(1) {
        sleep(1000);
    }

    zcm_destroy(zcm);
    return 0;
}
