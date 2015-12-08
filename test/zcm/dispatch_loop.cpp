// Test cases for zcm_run() dispatch style
#include <thread>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <unistd.h>

#include "zcm/zcm.h"
#include "util/TimeUtil.hpp"

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

static void test_run()
{
    zcm_t *zcm = zcm_create("ipc");
    assert(zcm);

    running = true;
    std::thread kill {killThread};
    std::thread ctrl {controlThread, zcm};

    zcm_run(zcm);

    running = false;
    ctrl.join();
    kill.join();

    zcm_destroy(zcm);
}

static void test_spawn1()
{
    zcm_t *zcm = zcm_create("ipc");
    assert(zcm);

    zcm_start(zcm);
    zcm_stop(zcm);

    zcm_destroy(zcm);
}

static void test_spawn2()
{
    zcm_t *zcm = zcm_create("ipc");
    assert(zcm);

    zcm_start(zcm);
    usleep(100*1000);
    zcm_stop(zcm);

    zcm_destroy(zcm);
}

static void test_handle()
{
    zcm_t *zcm = zcm_create("ipc");
    assert(zcm);

    running = true;
    std::thread kill {killThread};
    std::thread ctrl {controlThread, zcm};

    zcm_handle(zcm);

    running = false;
    ctrl.join();
    kill.join();

    zcm_destroy(zcm);
}

int main()
{
    test_run();
    test_spawn1();
    test_spawn2();
    test_handle();

    return 0;
}
