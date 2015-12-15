#include <zcm/zcm.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define CHANNEL "CHANNEL"
volatile bool running = true;

static void handler(const zcm_recv_buf_t *rbuf,
                    const char *channel, void *usr)
{
    printf("%s\n", rbuf->data);
    running = false;
}

int main()
{
    zcm_t *zcm = zcm_create("tcp://localhost:6700");
    assert(zcm);
    zcm_subscribe(zcm, CHANNEL, handler, NULL);

    while (running)
        zcm_handle(zcm);

    return 0;
}
