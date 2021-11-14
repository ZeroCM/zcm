#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "types/bitfield_t.h"

static bool quiet = false;

static void my_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                       const bitfield_t *msg, void *user)
{
    printf("Received message\n");
    if (quiet) return;

    printf("%d\n", msg->field1);
    printf("[");
    for (int j = 0; j < 2; ++j) {
        printf("[");
        for (int i = 0; i < 4; ++i) {
            printf("%d, ", msg->field2[j][i]);
        }
        printf("], ");
    }
    printf("]\n");
    printf("%d\n", msg->field3);
    printf("%d\n", msg->field4);
    printf("%d\n", msg->field5);
    printf("%d\n", msg->field6);
    printf("%d\n", msg->field7);
    printf("%d\n", msg->field8_dim1);
    printf("%d\n", msg->field8_dim2);
    printf("[");
    for (int j = 0; j < msg->field8_dim1; ++j) {
        printf("[");
        for (int i = 0; i < msg->field8_dim2; ++i) {
            printf("%d, ", msg->field8[j][i]);
        }
        printf("]");
    }
    printf("]\n");
    printf("%d\n", msg->field9);
    printf("%ld\n", msg->field10);
    printf("%d\n", msg->field11);
    printf("[");
    for (int l = 0; l < 3; ++l) {
        printf("[");
        for (int k = 0; k < 2; ++k) {
            printf("[");
            for (int j = 0; j < 2; ++j) {
                printf("[");
                for (int i = 0; i < 2; ++i) {
                    printf("%d, ", msg->field12[l][k][j][i]);
                }
                printf("], ");
            }
            printf("], ");
        }
        printf("], ");
    }
    printf("]\n");
    printf("%d\n", msg->field15);
    printf("%d\n", msg->field16);
    printf("%d\n", msg->field18);
    printf("%d\n", msg->field19);
    printf("%d\n", msg->field20);
    printf("%d\n", msg->field21);
    printf("%d\n", msg->field22);
    printf("%d\n", msg->field23);
    printf("%d\n", msg->field24);
    printf("%d\n", msg->field25);
    printf("%d\n", msg->field26);
    printf("%d\n", msg->field27);
    printf("%d\n", msg->field28);
    printf("%d\n", msg->field29);
    printf("%d\n", msg->field30);
    printf("%d\n", msg->field31);
    printf("%d\n", msg->field32);
    printf("%ld\n", msg->field33);
    printf("%ld\n", msg->field34);
}

int main(int argc, char *argv[])
{
    const char *channel = "BITFIELD";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            zcm_transport_help(stdout);
            return 0;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else { // assume channel
            channel = argv[i];
        }
    }

    zcm_t *zcm = zcm_create("");
    if(!zcm)
        return 1;

    bitfield_t_subscribe(zcm, channel, &my_handler, NULL);

    zcm_run(zcm);
    zcm_destroy(zcm);
    return 0;
}
