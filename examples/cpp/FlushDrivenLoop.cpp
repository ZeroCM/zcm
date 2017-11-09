#include <stdio.h>
#include <unistd.h>
#include <string>
#include <atomic>
#include <thread>
#include <zcm/zcm-cpp.hpp>
#include "types/example_t.hpp"
using namespace std;

static const uint16_t hz = 25;
static const uint16_t numInBlock = 25;
static const uint16_t numToRecv = 100;

class Handler
{
  private:
    atomic<size_t>& counter;
    bool            verbose;
  public:
    Handler(atomic<size_t>& counter, bool verbose) : counter(counter), verbose(verbose) {}
    ~Handler() {}

    void handleMessage(const zcm::ReceiveBuffer* rbuf,
                       const string& chan,
                       const example_t *msg)
    {
        ++counter;
        if (verbose) {
            printf("Received \"%s\" : %zu\n", chan.c_str(), counter.load());
        }
    }
};

void usage()
{
    printf("usage: ./flushdrivenloop <chanInput> <chanOutput> <order>\n");
}

int main(int argc, char *argv[])
{
    string chanI;
    string chanO;
    size_t order;

    if (argc < 4) { usage(); return 1; }
    if (strcmp(argv[1], "-h") == 0) { usage(); return 0; }

    chanI = argv[1];
    chanO = argv[2];
    order = atoi(argv[3]);

    zcm::ZCM zcm {""};
    if (!zcm.good()) return 1;

    atomic<size_t> count {0};
    Handler handler(count, true);
    zcm.subscribe(chanI, &Handler::handleMessage, &handler);

    zcm.setQueueSize(numInBlock * 4);

    example_t msg {};
    msg.timestamp = 0;

    zcm.start();
    //zcm.pause();

    auto publishBlock = [&](){
        size_t initCount = count;
        while (count == initCount) {
            zcm.publish(chanO, &msg);
            ++msg.timestamp;
            usleep(1e6 / hz);
        }
        //zcm.flush();
    };

    auto receiveBlock = [&](size_t blockNum){
        while (count < numInBlock * blockNum) {
            //zcm.flush();
            usleep(1e6 / hz);
        }
    };

    if (order == 1) publishBlock();

    size_t blockNum = 1;
    while (count < numToRecv) {
        receiveBlock(blockNum);
        publishBlock();
        ++blockNum;
    }
    // publish a few extra messages to make sure the other program exits
    publishBlock();

    printf("\nStopping\n");
    zcm.stop();

    return 0;
}
