#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

static char last_received = 0;

static void generic_handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    printf("%lu - %s: ", rbuf->utime, channel);
    for (size_t i = 0; i < rbuf->len; ++i) {
        printf("%c ", rbuf->data[i]);
        last_received = rbuf->data[i];
    }
    printf("\n");
    fflush(stdout);
}

int main(int argc, const char *argv[])
{
    const char* transports[2] = {"ipc", "inproc"};
    int i;
    for (i = 0; i < 2; ++i) {
        zcm_t *zcm = zcm_create(transports[i]);
        printf("Creating zcm %s\n", transports[i]);
        if (!zcm) {
            printf("Failed to create zcm\n");
            return 1;
        }

        char data[5] = {'a', 'b', 'c', 'd', 'e'};

        zcm_sub_t *sub = zcm_subscribe(zcm, "TEST", generic_handler, NULL);
        zcm_publish(zcm, "TEST", data, sizeof(char));

        // zmq sockets are documented as taking a small but perceptible amount of time
        // to actuall establish connection, so in order to actually receive messages
        // through them, we must wait for them to be set up
        usleep(200000);

        int j;
        for (j = 0; j < 5; ++j) {
            zcm_publish(zcm, "TEST", data+j, sizeof(char));
        }

        usleep(200000);

        zcm_unsubscribe(zcm, sub);

        printf("Cleaning up zcm %s\n", transports[i]);
        zcm_destroy(zcm);
    }

    return 0;
}
