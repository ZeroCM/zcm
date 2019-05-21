#include "zcm/zcm.h"
#include "types/example_t.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

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
    vprintf("%" PRIi64 " - %s: ", rbuf->recv_utime, channel);
    size_t i;
    for (i = 0; i < rbuf->data_size; ++i) {
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

static int  num_typed_received = 0;
static int  bytepacked_typed_received = 0;
static void example_t_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                              const example_t *msg, void *userdata)
{
    vprintf("%" PRIi64 " - %s: ", rbuf->recv_utime, channel);
    vprintf("%d", (int) msg->utime);
    bytepacked_typed_received |= (int) msg->utime;
    num_typed_received++;
    vprintf("\n");
    fflush(stdout);
}

int main(int argc, const char *argv[])
{
    size_t sleep_time = 200000;

    size_t i;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-h] [-v] [-s <sleep us>]\n", argv[0]);
            printf("       -h             help\n");
            printf("       -v             verbose\n");
            printf("       -s <sleep_us>  sleep time (def = 200000), increase for valgrind\n");
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            if (++i == argc) {
                fprintf(stderr, "option -s requires argument\n");
                return 1;
            } else {
                sleep_time = atoi(argv[i]);
            }
        }
    }

    const char* transports[2] = {"ipc", "inproc"};
    for (i = 0; i < 2; ++i) {
        zcm_t *zcm = zcm_create_from_url(transports[i]);
        vprintf("Creating zcm %s\n", transports[i]);
        if (!zcm) {
            fprintf(stderr, "Failed to create zcm\n");
            return 1;
        }

        // TEST RAW DATA TRANSMISSION /////////////////////////////////////////
        num_received = 0;
        bytepacked_received = 0;
        zcm_sub_t *sub = zcm_subscribe(zcm, "TEST", generic_handler, NULL);
        zcm_publish(zcm, "TEST", data, sizeof(char));

        // zmq sockets are documented as taking a small but perceptible amount of time
        // to actuall establish connection, so in order to actually receive messages
        // through them, we must wait for them to be set up
        usleep(sleep_time);

        vprintf("Starting zcm receive %s\n", transports[i]);
        zcm_start(zcm);

        size_t j;
        for (j = 0; j < NUM_DATA; ++j) {
            zcm_publish(zcm, "TEST", data+j, sizeof(char));
        }

        usleep(sleep_time);
        vprintf("Stopping zcm receive %s\n", transports[i]);
        zcm_stop(zcm);

        vprintf("Unsubscribing zcm %s\n", transports[i]);
        zcm_unsubscribe(zcm, sub);

        for (j = 0; j < NUM_DATA; ++j) {
            if (!(bytepacked_received & 1 << j)) {
                fprintf(stderr, "%s: Missed a message: %c\n", transports[i], data[j]);
                fflush(stderr);
                ++retval;
            }
        }
        if (num_received != NUM_DATA && num_received != NUM_DATA+1) {
            fprintf(stderr, "%s: Received an unexpected number of messages: %d\n",
                    transports[i], num_received);
            fflush(stderr);
            ++retval;
        }


        // TEST TYPED DATA TRANSMISSION ///////////////////////////////////////
        num_typed_received = 0;
        bytepacked_typed_received = 0;

        example_t ex_data = {
               .utime = 1 << 0,
               .position = { 1, 2, 3 },
               .orientation = { 1, 0, 0, 0 },
        };
        ex_data.num_ranges = 100;
        ex_data.ranges = calloc(ex_data.num_ranges, sizeof(int16_t));
        ex_data.name = "example string";
        ex_data.enabled = 1;

        example_t_subscription_t *ex_sub = example_t_subscribe(zcm, "EXAMPLE",
                                                               example_t_handler, NULL);
        example_t_publish(zcm, "EXAMPLE", &ex_data);

        // zmq sockets are documented as taking a small but perceptible amount of time
        // to actuall establish connection, so in order to actually receive messages
        // through them, we must wait for them to be set up
        usleep(sleep_time);

        vprintf("Starting zcm receive %s\n", transports[i]);
        zcm_start(zcm);

        for (j = 0; j < NUM_DATA; ++j) {
            ex_data.utime = 1 << j;
            example_t_publish(zcm, "EXAMPLE", &ex_data);
        }

        usleep(sleep_time);
        vprintf("Stopping zcm receive %s\n", transports[i]);
        zcm_stop(zcm);

        vprintf("Unsubscribing zcm %s\n", transports[i]);
        example_t_unsubscribe(zcm, ex_sub);
        free(ex_data.ranges);

        for (j = 0; j < NUM_DATA; ++j) {
            if (!(bytepacked_typed_received & 1 << j)) {
                fprintf(stderr, "%s: Missed an example message: %d\n", transports[i], 1 << j);
                fflush(stderr);
                ++retval;
            }
        }
        if (num_typed_received != NUM_DATA && num_typed_received != NUM_DATA+1) {
            fprintf(stderr, "%s: Received an unexpected number of messages: %d\n",
                    transports[i], num_typed_received);
            fflush(stderr);
            ++retval;
        }


        vprintf("Cleaning up zcm %s\n", transports[i]);
        zcm_destroy(zcm);
    }

    return retval;
}
