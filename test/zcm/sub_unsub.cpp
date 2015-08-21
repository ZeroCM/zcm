#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>

#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_map>
using namespace std;

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
    string transports[]{"ipc", "inproc"};
    for (auto s : transports) {
        zcm_t *zcm = zcm_create(s.c_str());
        cout << "Creating zcm " << s << endl;
        if (!zcm) {
            cout << "Failed to create zcm" << endl;
            return 1;
        }

        char data[5]{'a', 'b', 'c', 'd', 'e'};

        auto *sub = zcm_subscribe(zcm, "TEST", generic_handler, nullptr);
        zcm_publish(zcm, "TEST", data, sizeof(char));

        // zmq sockets are documented as taking a small but perceptible amount of time
        // to actuall establish connection, so in order to actually receive messages
        // through them, we must wait for them to be set up
        usleep(200000);

        for (int i = 0; i < 5; ++i) {
            zcm_publish(zcm, "TEST", data+i, sizeof(char));
        }

        usleep(200000);

        zcm_unsubscribe(zcm, sub);

        cout << "Cleaning up zcm " << s << endl;
        zcm_destroy(zcm);
    }

    return 0;
}
