#include "zcm.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static int handler(zcm_t *zcm, const char *channel, char *data, size_t len, void *usr)
{
    printf("Got message on '%s': %s\n", channel, data);
    return 0;
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
