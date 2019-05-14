#include <zcm/zcm.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define URL "udpm://239.255.76.67:7667?ttl=0"
#define CHANNEL "HIGHRATE_TEST"
#define DATASZ (4096*32)
#define N 1000000
#define SLEEPUS 100

static size_t recv_count = 0;
static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    recv_count++;
}

int main(int argc, char *argv[])
{
    uint8_t *data = malloc(DATASZ);
    memset(data, 0, DATASZ);

    zcm_t *zcm = zcm_create(URL);
    assert(zcm);

    zcm_subscribe(zcm, CHANNEL, handler, NULL);
    zcm_start(zcm);

    for (size_t i = 0; i < N; i++) {
        zcm_publish(zcm, CHANNEL, data, DATASZ);
        usleep(SLEEPUS);
    }

    usleep(100000);
    zcm_stop(zcm);
    zcm_destroy(zcm);

    printf("Message success: %d/%d\n", (int)recv_count, N);

    free(data);
    return 0;
}
