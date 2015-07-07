#include "zcm.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void handler(zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    printf("Got message on '%s': %s\n", channel, rbuf->data);
}

int main()
{
    zcm_t *zcm = zcm_create();
    zcm_subscribe(zcm, "foobar", handler, NULL);

    while (1) {
        sleep(1000);
    }

    return 0;
}
