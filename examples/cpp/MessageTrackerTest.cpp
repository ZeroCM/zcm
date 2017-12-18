#include <iostream>
#include <sys/time.h>
#include <unistd.h>

#include <zcm/zcm-cpp.hpp>
#include <zcm/message_tracker.hpp>

#include "types/example_t.hpp"

using namespace std;

atomic_bool done {false};

static void callback(example_t* msg, uint64_t utime, void* usr)
{
    done = true;
    delete msg;
}

int main(int argc, char *argv[])
{
    zcm::ZCM zcmLocal;
    zcm::MessageTracker<example_t> mt(&zcmLocal, "EXAMPLE", 0.25, 1, callback);
    zcmLocal.start();
    cout << "Waiting to receive EXAMPLE message" << endl;
    auto tmp = mt.get();
    while (!done) usleep(1e5);
    cout << "Success!" << endl;
    zcmLocal.stop();
    delete tmp;
}
