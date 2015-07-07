#include "zcm.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    zcm_t *zcm = zcm_create();

    char data[] = "Foobar Data";
    while (1) {
        zcm_publish(zcm, "foobar", data, sizeof(data));
        usleep(250000);
    }

    return 0;
}
