#ifndef SUBUNSUBCTEST_HPP
#define SUBUNSUBCTEST_HPP

#include "zcm/zcm.h"
#include "cxxtest/TestSuite.h"
#include "types/example_t.h"

#include <inttypes.h>
#include <unistd.h>



using namespace std;

static int  num_received = 0;
static int  bytepacked_received = 0;
#define NUM_DATA 5

static uint8_t data[NUM_DATA] = {'a', 'b', 'c', 'd', 'e'};
static void generic_handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    printf("%" PRIi64 " - %s: ", rbuf->recv_utime, channel);
    size_t i;
    for (i = 0; i < rbuf->data_size; ++i) {
        printf("%c ", rbuf->data[i]);

        num_received++;
        size_t j;
        for (j = 0; j < NUM_DATA; ++j) {
            if (rbuf->data[i] == data[j]) {
                bytepacked_received |= 1 << j;
            }
        }
    }
    printf("\n");
    fflush(stdout);
}

static int  num_typed_received = 0;
static int  bytepacked_typed_received = 0;
static void example_t_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                              const example_t *msg, void *userdata)
{
    printf("%" PRIi64 " - %s: ", rbuf->recv_utime, channel);
    printf("%d", (int) msg->utime);
    bytepacked_typed_received |= (int) msg->utime;
    num_typed_received++;
    printf("\n");
    fflush(stdout);
}

class SubUnsubCTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testSubUnsub() {
        size_t sleep_time = 200000;


        for (string transport : {"ipc", "inproc", "udpm://239.255.76.67:7667?ttl=0"}) {
            printf("Creating zcm %s\n", transport.c_str());
            zcm_t *zcm = zcm_create(transport.c_str());
            TSM_ASSERT("Failed to create zcm", zcm)


            // TEST RAW DATA TRANSMISSION /////////////////////////////////////////
            printf("\nTest raw data transmission\n");
            num_received = 0;
            bytepacked_received = 0;
            zcm_sub_t *sub = zcm_subscribe(zcm, "TEST", generic_handler, NULL);
            TSM_ASSERT("Subscription failed", sub);
            if(transport == "ipc") // TODO: it is clearly a bug of the ipc transport that it needs this first message to be published (it never arrives)
                TSM_ASSERT_EQUALS("Publishing failed!", zcm_publish(zcm, "TEST", data, sizeof(char)), ZCM_EOK);

            // zmq sockets are documented as taking a small but perceptible amount of time
            // to actuall establish connection, so in order to actually receive messages
            // through them, we must wait for them to be set up
            usleep(sleep_time);

            printf("Starting zcm receive %s\n", transport.c_str());
            zcm_start(zcm);

            size_t j;
            for (j = 0; j < NUM_DATA; ++j) {
                TSM_ASSERT_EQUALS("Publishing failed!", zcm_publish(zcm, "TEST", data+j, sizeof(char)), ZCM_EOK);
            }

            usleep(sleep_time);
            printf("Stopping zcm receive %s\n", transport.c_str());
            zcm_stop(zcm);

            printf("Unsubscribing zcm %s\n", transport.c_str());
            zcm_unsubscribe(zcm, sub);

            for (j = 0; j < NUM_DATA; ++j) {
                TSM_ASSERT("We missed a message", bytepacked_received & 1 << j)
            }

            TSM_ASSERT_EQUALS("Received an unexpected number of messages!", num_received, NUM_DATA);

            // TEST TYPED DATA TRANSMISSION ///////////////////////////////////////
            printf("\nTest typed data transmission\n");
            num_typed_received = 0;
            bytepacked_typed_received = 0;

            example_t ex_data = {
                   .utime = 1 << 0,
                   .position = { 1, 2, 3 },
                   .orientation = { 1, 0, 0, 0 },
            };
            ex_data.num_ranges = 100;
            ex_data.ranges = (int16_t*) calloc(ex_data.num_ranges, sizeof(int16_t));
            char name[] = "example string";
            ex_data.name = name;
            ex_data.enabled = 1;

            example_t_subscription_t *ex_sub = example_t_subscribe(zcm, "EXAMPLE",
                                                                   example_t_handler, NULL);
            TSM_ASSERT("Failed to subscribe", ex_sub);
            if(transport == "ipc") // TODO: it is clearly a bug of the ipc transport that it needs this first message to be published (it never arrives)
                TSM_ASSERT_EQUALS("Failed to publish", example_t_publish(zcm, "EXAMPLE", &ex_data), ZCM_EOK);

            // zmq sockets are documented as taking a small but perceptible amount of time
            // to actuall establish connection, so in order to actually receive messages
            // through them, we must wait for them to be set up
            usleep(sleep_time);

            printf("Starting zcm receive %s\n", transport.c_str());
            zcm_start(zcm);

            for (j = 0; j < NUM_DATA; ++j) {
                ex_data.utime = 1 << j;
                assert(example_t_publish(zcm, "EXAMPLE", &ex_data) == ZCM_EOK);
            }

            usleep(sleep_time);
            printf("Stopping zcm receive %s\n", transport.c_str());
            zcm_stop(zcm);

            printf("Unsubscribing zcm %s\n", transport.c_str());
            example_t_unsubscribe(zcm, ex_sub);
            free(ex_data.ranges);


            for (size_t j = 0; j < NUM_DATA; ++j) {
                TSM_ASSERT("We missed a message", bytepacked_received & 1 << j)
            }

            TSM_ASSERT_EQUALS("Received an unexpected number of messages!", num_received, NUM_DATA);

            printf("Cleaning up zcm %s\n", transport.c_str());
            zcm_destroy(zcm);
        }
    }

};
#endif // SUBUNSUBCTEST_HPP
