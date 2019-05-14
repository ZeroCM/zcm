#include "zcm/zcm.h"
#include <unistd.h>
#include <thread>
#include <cassert>

#define CHANNEL "TEST_CHANNEL"
#define N 10

static volatile size_t numrecv = 0;
static volatile bool subrunning = false;

static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    numrecv++;
}

void subfunc()
{
    zcm_t *zcm = zcm_create("udpm://239.255.76.67:7667?ttl=0");
    zcm_subscribe(zcm, CHANNEL, handler, NULL);

    zcm_start(zcm);
    subrunning = true;
    while (subrunning) {
        usleep(100000);
    }
    zcm_stop(zcm);

    zcm_destroy(zcm);
}

int main()
{
    // start sub
    std::thread subthread {subfunc};

    // busy wait for subthread
    while (!subrunning) {}

    // do pub
    uint8_t data = 'A';
    for (size_t i = 0; i < N; i++) {
        // create, send, and destroy
        zcm_t *zcm = zcm_create("udpm://239.255.76.67:7667?ttl=0");
        assert(zcm);
        zcm_publish(zcm, CHANNEL, &data, 1);
        zcm_flush(zcm);
        zcm_destroy(zcm);
    }


    // shutdown
    subrunning = false;
    subthread.join();

    // report
    if (numrecv != N) {
        printf("Received %zu/%d\n", numrecv, N);
        return 1;
    } else {
        return 0;
    }
}
