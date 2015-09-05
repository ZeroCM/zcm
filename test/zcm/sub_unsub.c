#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define vprintf(...) do { \
    if (verbose) { \
        printf(__VA_ARGS__); \
    } \
} while(0)

#define NUM_DATA 5
static int  verbose = 0;
static int  retval = 0;
static int  num_received = 0;
static int  bytepacked_received = 0;
static char data[NUM_DATA] = {'a', 'b', 'c', 'd', 'e'};

static void generic_handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    vprintf("%lu - %s: ", rbuf->utime, channel);
    size_t i;
    for (i = 0; i < rbuf->len; ++i) {
        vprintf("%c ", rbuf->data[i]);

        num_received++;
        size_t j;
        for (j = 0; j < NUM_DATA; ++j) {
            if (rbuf->data[i] == data[j]) {
                bytepacked_received |= 1 << j;
            }
        }
    }
    vprintf("\n");
    fflush(stdout);
}

int main(int argc, const char *argv[])
{
    size_t i;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-h] [-v]\n", argv[0]);
            printf("       -h help\n");
            printf("       -v verbose\n");
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        }
    }

    const char* transports[2] = {"ipc", "inproc"};
    for (i = 0; i < 2; ++i) {
        zcm_t *zcm = zcm_create(transports[i]);
        vprintf("Creating zcm %s\n", transports[i]);
        if (!zcm) {
            vprintf("Failed to create zcm\n");
            return 1;
        }

        num_received = 0;
        bytepacked_received = 0;
        zcm_sub_t *sub = zcm_subscribe(zcm, "TEST", generic_handler, NULL);
        zcm_publish(zcm, "TEST", data, sizeof(char));

        // zmq sockets are documented as taking a small but perceptible amount of time
        // to actuall establish connection, so in order to actually receive messages
        // through them, we must wait for them to be set up
        usleep(200000);

        size_t j;
        for (j = 0; j < NUM_DATA; ++j) {
            zcm_publish(zcm, "TEST", data+j, sizeof(char));
        }

        usleep(200000);

        zcm_unsubscribe(zcm, sub);

        for (j = 0; j < NUM_DATA; ++j) {
            if (!(bytepacked_received & 1 << j)) {
                fprintf(stderr, "%s: Missed a message: %c\n", transports[i], data[j]);
                ++retval;
            }
            if (num_received != NUM_DATA && num_received != NUM_DATA+1) {
                fprintf(stderr, "%s: Received an unexpected number of messages\n", transports[i]);
                ++retval;
            }
        }

        vprintf("Cleaning up zcm %s\n", transports[i]);
        zcm_destroy(zcm);
    }

    return retval;
}
