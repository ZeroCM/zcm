#include <iostream>

#include <zcm/zcm-cpp.hpp>
#include <zcm/message_tracker.hpp>

#include "types/example_t.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    zcm::ZCM zcmLocal;
    MessageTracker<example_t> mt(&zcmLocal, "EXAMPLE");
    zcmLocal.start();
    cout << "Waiting to receive EXAMPLE message" << endl;
    auto tmp = mt.get();
    cout << "Success!" << endl;
    zcmLocal.stop();
    delete tmp;
}
