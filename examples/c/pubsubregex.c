#include <stdio.h>
#include <unistd.h>
#include <zcm/zcm.h>

void handleMessage(const zcm_recv_buf_t* rbuf,
                   const char* channel, void* usr)
{
    printf("Received message on channel \"%s\"\n", channel);
}

int main(int argc, char *argv[])
{
    zcm_t *zcm = zcm_create("");
    if (!zcm) return 1;

    zcm_sub_t* subs1 = zcm_subscribe(zcm, "EXAMPLE_1.*", handleMessage, NULL);
    zcm_sub_t* subs2 = zcm_subscribe(zcm, "EXAMPLE_2.*", handleMessage, NULL);
    zcm_sub_t* subs3 = zcm_subscribe(zcm, "EXAMPLE_3_", handleMessage, NULL);

    zcm_start(zcm);

    zcm_publish(zcm, "EXAMPLE_1_", NULL, 0);
    zcm_publish(zcm, "EXAMPLE_2_", NULL, 0);
    zcm_publish(zcm, "EXAMPLE_3_", NULL, 0);

    usleep(1e6);

    zcm_publish(zcm, "EXAMPLE_1_", NULL, 0);
    zcm_publish(zcm, "EXAMPLE_2_", NULL, 0);
    zcm_publish(zcm, "EXAMPLE_3_", NULL, 0);

    zcm_flush(zcm);
    usleep(1e6);

    zcm_stop(zcm);

    assert(zcm_unsubscribe(zcm, subs1) == ZCM_EOK);
    assert(zcm_unsubscribe(zcm, subs2) == ZCM_EOK);
    assert(zcm_unsubscribe(zcm, subs3) == ZCM_EOK);

    zcm_destroy(zcm);

    return 0;
}
