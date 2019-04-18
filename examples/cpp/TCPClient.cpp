#include <zcm/zcm-cpp.hpp>

#include <iostream>
#include <sys/unistd.h>

using namespace std;

#define CHANNEL "CHANNEL"
volatile bool running = true;

static void handler(const zcm::ReceiveBuffer* rbuf, const std::string& channel, void* usr)
{
    cout << "Received message on channel " << channel << endl;
    //running = false;
}

int main()
{
    zcm::ZCM z("tcp://localhost:6700");
    z.subscribe(CHANNEL, handler, NULL);

    z.start();
    while (running) {
        //z.publish("TEST", (const uint8_t*) "MESSAGE", 8);
        usleep(1e5);
    }
    z.stop();

    return 0;
}
