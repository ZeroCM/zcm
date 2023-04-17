#ifndef SUBUNSUBCPPTEST_H
#define SUBUNSUBCPPTEST_H

#include "cxxtest/TestSuite.h"

#include "zcm/zcm-cpp.hpp"
#include "types/example_t.hpp"

#include <cinttypes>
#include <unistd.h>

#include "multi_file.hpp"


using namespace std;

#define NUM_DATA 5
static uint8_t data[NUM_DATA] = {'a', 'b', 'c', 'd', 'e'};
static const size_t sleep_time = 5e5;

class Handler
{
  public:
    int  num_received = 0;
    int  bytepacked_received = 0;

    void generic_handle(const zcm::ReceiveBuffer *rbuf, const string& channel)
    {
        printf("%" PRIi64 " - %s: ", rbuf->recv_utime, channel.c_str());
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

    int  num_typed_received = 0;
    int  bytepacked_typed_received = 0;
    void example_t_handle(const zcm::ReceiveBuffer *rbuf, const string& channel,
                          const example_t *msg)
    {
        printf("%" PRIi64 " - %s: ", rbuf->recv_utime, channel.c_str());
        printf("%d", (int) msg->utime);
        bytepacked_typed_received |= (int) msg->utime;
        num_typed_received++;
        printf("\n");
        MyStruct::greet(false, msg);
        fflush(stdout);
    }
};

class SubUnsubCPPTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testPubSub()
    {
        Handler handler;

        for (string transport : {"ipc", "inproc", "udpm://239.255.76.67:7667?ttl=0"}) {
            zcm::ZCM zcm(transport);
            printf("Creating zcm %s\n", transport.c_str());
            TSM_ASSERT("Failed to create ZCM", zcm.good());


            // TEST RAW DATA TRANSMISSION /////////////////////////////////////////
            handler.num_received = 0;
            handler.bytepacked_received = 0;
            zcm::Subscription *sub = zcm.subscribe("TEST", &Handler::generic_handle, &handler);
            TSM_ASSERT("Failed to subscribe", sub);

            if(transport == "ipc") // TODO: it is clearly a bug of the ipc transport that it needs this first message to be published (it never arrives)
                zcm.publish("TEST", data, sizeof(char));

            // zmq sockets are documented as taking a small but perceptible amount of time
            // to actuall establish connection, so in order to actually receive messages
            // through them, we must wait for them to be set up
            usleep(sleep_time);

            printf("Starting zcm receive %s\n", transport.c_str());
            zcm.start();

            for (size_t j = 0; j < NUM_DATA; ++j) {
                zcm.publish("TEST", data+j, sizeof(char));
            }

            usleep(sleep_time);
            printf("Stopping zcm receive %s\n", transport.c_str());
            zcm.stop();

            printf("Unsubscribing zcm %s\n", transport.c_str());
            zcm.unsubscribe(sub);

            for (size_t j = 0; j < NUM_DATA; ++j) {
                TSM_ASSERT("We missed a message", handler.bytepacked_received & 1 << j)
            }

            TSM_ASSERT_EQUALS("Received an unexpected number of messages!", handler.num_received, NUM_DATA);

            // TEST TYPED DATA TRANSMISSION ///////////////////////////////////////
            handler.num_typed_received = 0;
            handler.bytepacked_typed_received = 0;

            example_t ex_data;
            ex_data.utime = 1 << 0;
            ex_data.position[0] = 1;
            ex_data.position[1] = 2;
            ex_data.position[2] = 3;
            ex_data.orientation[0] = 1;
            ex_data.orientation[1] = 0;
            ex_data.orientation[2] = 0;
            ex_data.orientation[3] = 0;
            ex_data.num_ranges = 100;
            ex_data.ranges.resize(ex_data.num_ranges);
            ex_data.name = "example string";
            ex_data.enabled = 1;

            zcm::Subscription *ex_sub = zcm.subscribe("EXAMPLE", &Handler::example_t_handle, &handler);
            if(transport == "ipc") // TODO: it is clearly a bug of the ipc transport that it needs this first message to be published (it never arrives)
                zcm.publish("EXAMPLE", &ex_data);


            // zmq sockets are documented as taking a small but perceptible amount of time
            // to actuall establish connection, so in order to actually receive messages
            // through them, we must wait for them to be set up
            usleep(sleep_time);

            printf("Starting zcm receive %s\n", transport.c_str());
            zcm.start();

            for (size_t j = 0; j < NUM_DATA; ++j) {
                ex_data.utime = 1 << j;
                zcm.publish("EXAMPLE", &ex_data);
            }

            usleep(sleep_time);
            printf("Stopping zcm receive %s\n", transport.c_str());
            zcm.stop();

            printf("Unsubscribing zcm %s\n", transport.c_str());
            zcm.unsubscribe(ex_sub);

            for (size_t j = 0; j < NUM_DATA; ++j) {
                TSM_ASSERT("We missed a message", handler.bytepacked_received & 1 << j)
            }

            TSM_ASSERT_EQUALS("Received an unexpected number of messages!", handler.num_received, NUM_DATA);

            printf("Cleaning up zcm %s\n", transport.c_str());
        }
    }

    void testUnsubRegexExplicit()
    {
        for (string transport : {"ipc", "inproc", "udpm://239.255.76.67:7667?ttl=0"}) {
            zcm::ZCM zcm(transport);
            printf("Creating zcm %s\n", transport.c_str());
            TSM_ASSERT("Failed to create ZCM", zcm.good());


            // TEST RAW DATA TRANSMISSION /////////////////////////////////////////
            Handler handler1;
            handler1.num_received = 0;
            handler1.bytepacked_received = 0;
            zcm::Subscription *sub1 = zcm.subscribe("TES.*", &Handler::generic_handle, &handler1);
            TSM_ASSERT("Failed to subscribe", sub1);

            Handler handler2;
            handler2.num_received = 0;
            handler2.bytepacked_received = 0;
            zcm::Subscription *sub2 = zcm.subscribe("TEST.*", &Handler::generic_handle, &handler2);
            TSM_ASSERT("Failed to subscribe", sub2);

            Handler handler3;
            handler3.num_received = 0;
            handler3.bytepacked_received = 0;
            zcm::Subscription *sub3 = zcm.subscribe("TEST", &Handler::generic_handle, &handler3);
            TSM_ASSERT("Failed to subscribe", sub3);

            printf("Unsubscribing zcm %s\n", transport.c_str());
            zcm.unsubscribe(sub1);
            zcm.unsubscribe(sub2);

            if(transport == "ipc") // TODO: it is clearly a bug of the ipc transport that it needs this first message to be published (it never arrives)
                zcm.publish("TEST", data, sizeof(char));

            // zmq sockets are documented as taking a small but perceptible amount of time
            // to actuall establish connection, so in order to actually receive messages
            // through them, we must wait for them to be set up
            usleep(sleep_time);

            printf("Starting zcm receive %s\n", transport.c_str());
            zcm.start();

            for (size_t j = 0; j < NUM_DATA; ++j) {
                zcm.publish("TEST", data+j, sizeof(char));
            }

            usleep(sleep_time);
            printf("Stopping zcm receive %s\n", transport.c_str());
            zcm.stop();

            zcm.unsubscribe(sub3);

            for (size_t j = 0; j < NUM_DATA; ++j) {
                TSM_ASSERT("We missed a message", handler3.bytepacked_received & 1 << j)
            }

            TSM_ASSERT_EQUALS("Received an unexpected number of messages!", handler3.num_received, NUM_DATA);
        }
    }

};

#endif // SUBUNSUBCPPTEST_H
