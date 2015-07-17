#include <stdio.h>
#include <unistd.h>
#include <zcm/zcm.h>

static void handler(const zcm_recv_buf_t *rbuf, const char *channel,
                    void *ser)
{
    printf("Got message on channel \"%s\"\n", channel);
}

int main(int argc, char *argv[])
{
    zcm_t *zcm = zcm_create();
    if(!zcm)
        return 1;

    zcm_subscribe(zcm, ".*", &handler, NULL);

    while(1)
        zcm_handle(zcm);

    zcm_destroy(zcm);
    return 0;
}
