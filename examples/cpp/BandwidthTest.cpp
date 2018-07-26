#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include <mutex>

#include <zcm/zcm-cpp.hpp>
#include <zcm/util/Filter.hpp>

using namespace std;
using namespace zcm;

static atomic_bool done {false};
static void sighandler(int s) { done = true; }

static mutex msgLock;
static int ctr = 0;

static Filter hzFilter(Filter::convergenceTimeToNatFreq(20, 0.8), 0.8);

static void msg_handler(const ReceiveBuffer* rbuf, const std::string& channel, void* usr)
{
    unique_lock<mutex> lk(msgLock);
    ++ctr;
    static uint64_t lastHostUtime = std::numeric_limits<uint64_t>::max();
    if (lastHostUtime != std::numeric_limits<uint64_t>::max()) {
        double obs = rbuf->recv_utime - lastHostUtime;
        hzFilter(obs, 1);
    }
    lastHostUtime = rbuf->recv_utime;
}

int main(int argc, char *argv[])
{
    signal(SIGINT,  sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    if (argc != 2) {
        cerr << "usage ./bandwidth-test <channel_name>" << endl;
        return 1;
    }

    ZCM zcmL;
    if (!zcmL.good()) {
        cerr << "Unable to open zcm" << endl;
        return 2;
    }

    auto subs = zcmL.subscribe(argv[1], msg_handler, nullptr);

    zcmL.start();

    while (!done) {
        {
            unique_lock<mutex> lk(msgLock);
            cout << "Num messages received: " << ctr << "    "
                 << " at " << 1e6 / hzFilter.lowPass() << " Hz" << endl;
        }
        usleep(1e6);
    }

    zcmL.stop();

    zcmL.unsubscribe(subs);
    return 0;
}
