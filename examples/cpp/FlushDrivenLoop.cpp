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

    example_t msg {};
    msg.timestamp = 0;

    zcm.start();
    zcm.publish(chanO, &msg);
    zcm.flush();
    usleep(1e6);

    zcm.setQueueSize(numInBlock * 4);
    zcm.pause();

    auto publishBlock = [&](){
        for (size_t i = 0; i < numInBlock / 2; ++i) {
            zcm.publish(chanO, &msg);
            ++msg.timestamp;
            zcm.flush();
            usleep(1e6 / hz);
        }
        size_t initCount = count;
        for (size_t i = 0; i < numInBlock; ++i) {
            if (count != initCount) break;
            zcm.publish(chanO, &msg);
            ++msg.timestamp;
            zcm.flush();
            usleep(1e6 / hz);
        }
    };

    auto receiveBlock = [&](size_t blockNum){
        count = (blockNum - 1) * numInBlock;
        while (count < numInBlock * blockNum) {
            zcm.flush();
            usleep(1e6 / hz);
        }
        printf("Finished receiving block %zu\n", blockNum);
    };

    if (order == 1) publishBlock();

    size_t blockNum = 1;
    while (count < numToRecv) {
        printf("\nWaiting on block %zu\n", blockNum);
        // This is literally just trying to make the queue resize mess up
        zcm.setQueueSize(numInBlock * 4 + (blockNum % 2) ? numInBlock : -numInBlock);
        receiveBlock(blockNum);
        publishBlock();
        ++blockNum;
    }

    printf("\nStopping\n");
    zcm.stop();

    return 0;
}
