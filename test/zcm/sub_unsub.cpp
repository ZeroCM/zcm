#include "zcm/zcm-cpp.hpp"

#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <string>

using namespace std;

#define vprintf(...) do { \
    if (verbose) { \
        printf(__VA_ARGS__); \
    } \
} while(0)

#define NUM_DATA 5
static bool verbose = false;
static int  retval = 0;
static int  num_received = 0;
static int  bytepacked_received = 0;
static char data[NUM_DATA] = {'a', 'b', 'c', 'd', 'e'};

class Handler
{
  public:
    void generic_handle(const zcm::ReceiveBuffer *rbuf, const string& channel)
    {
        vprintf("%lu - %s: ", rbuf->recv_utime, channel.c_str());
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
};

int main(int argc, const char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "-h") {
            cout << "Usage: " << argv[0] << " [-h] [-v]\n"
                 << "       -h help\n"
                 << "       -v verbose\n";
            return 0;
        } else if (string(argv[i]) == "-v") {
            verbose = true;
        }
    }

    Handler handler;

    string transports[2] = {"ipc", "inproc"};
    for (size_t i = 0; i < 2; ++i) {
        zcm::ZCM zcm(transports[i]);
        vprintf("Creating zcm %s\n", transports[i].c_str());
        if (!zcm.good()) {
            cerr << "Failed to create zcm\n";
            return 1;
        }

        num_received = 0;
        bytepacked_received = 0;
        zcm::Subscription *sub = zcm.subscribe("TEST", &Handler::generic_handle, &handler);
        zcm.publish("TEST", data, sizeof(char));

        // zmq sockets are documented as taking a small but perceptible amount of time
        // to actuall establish connection, so in order to actually receive messages
        // through them, we must wait for them to be set up
        usleep(200000);

        vprintf("Starting zcm receive %s\n", transports[i].c_str());
        zcm.start();

        for (size_t j = 0; j < NUM_DATA; ++j) {
            zcm.publish("TEST", data+j, sizeof(char));
        }

        usleep(200000);
        vprintf("Stopping zcm receive %s\n", transports[i].c_str());
        zcm.stop();

        zcm.unsubscribe(sub);

        for (size_t j = 0; j < NUM_DATA; ++j) {
            if (!(bytepacked_received & 1 << j)) {
                cerr << transports[i] << ": Missed a message: " << data[j] << endl;
                ++retval;
            }
        }
        if (num_received != NUM_DATA && num_received != NUM_DATA+1) {
            cerr << transports[i] << ": Received an unexpected number of messages" << endl;
            ++retval;
        }

        vprintf("Cleaning up zcm %s\n", transports[i].c_str());
    }

    return 0;
}
