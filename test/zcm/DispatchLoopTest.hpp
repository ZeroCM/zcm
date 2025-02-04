#ifndef DISPATCHLOOPTEST_H
#define DISPATCHLOOPTEST_H

#include <thread>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <unistd.h>

#include "cxxtest/TestSuite.h"
#include "zcm/zcm.h"
#include "util/TimeUtil.hpp"



using namespace std;

static std::atomic<bool> running;
static constexpr u64 TIMEOUT = 1000000; // 1 sec

static void killThread()
{
    u64 start = TimeUtil::utime();
    while (running) {
        u64 now = TimeUtil::utime();
        u64 dt = now - start;
        if (dt > TIMEOUT) {
            printf("Timeout! Test Failed.\n");
            exit(1);
        }
        usleep(1000);
    }
}

static void controlThread(zcm_t *zcm)
{
    // sleep 10 ms and then shut down zcm
    usleep(10000);
    zcm_stop(zcm);
}

class DispatchLoopTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testRun() {
        zcm_t *zcm = zcm_create("ipc");
        TS_ASSERT(zcm);

        running = true;
        std::thread kill {killThread};
        std::thread ctrl {controlThread, zcm};

        zcm_run(zcm);

        running = false;
        ctrl.join();
        kill.join();

        zcm_destroy(zcm);
    }

    void testSpawn1() {
        zcm_t *zcm = zcm_create("ipc");
        TS_ASSERT(zcm);

        running = true;
        std::thread kill {killThread};
        zcm_start(zcm);
        zcm_stop(zcm);
        running = false;

        kill.join();
        zcm_destroy(zcm);
    }

    void testSpawn2() {
        zcm_t *zcm = zcm_create("ipc");
        TS_ASSERT(zcm);

        running = true;
        std::thread kill {killThread};
        zcm_start(zcm);
        usleep(100*1000);
        zcm_stop(zcm);
        running = false;

        kill.join();
        zcm_destroy(zcm);
    }

    void testHandle() {
        zcm_t *zcm = zcm_create("ipc");
        TS_ASSERT(zcm);

        running = true;
        std::thread kill {killThread};
        std::thread ctrl {controlThread, zcm};

        zcm_handle(zcm, 0);

        running = false;
        ctrl.join();
        kill.join();

        zcm_destroy(zcm);
    }
};

#endif // DISPATCHLOOPTEST_H
