#include <zcm/zcm-cpp.hpp>

#include <iostream>
#include <vector>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include "types/example_t.hpp"

using namespace std;

static volatile int running = 1;
static void sighandler(int signo) { running = 0; }

static void handler(const zcm::ReceiveBuffer* rbuf, const std::string& channel, void* usr)
{
    cout << "Received message from client "
         << "0x" << std::hex << usr
         << " on channel " << channel
         << endl;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    example_t msg {};

    zcm::ZCMServer svr("tcp://localhost:6700");

    vector<zcm::ZCM*> clients {};

    cout << "Awaiting connections" << endl;

    while (running) {
        zcm::ZCM* client = svr.accept(500);
        if (client && client->good()) {
            cout << "Client connected" << endl;
            clients.push_back(client);
            client->subscribe(".*", handler, client);
            client->start();
            client->publish("CHANNEL", &msg);
            client->flush();
        }
    }

    cout << "Cleaning up" << endl;

    while (!clients.empty()) {
        zcm::ZCM* client = clients.back();
        delete client;
        clients.pop_back();
    }

    return 0;
}
