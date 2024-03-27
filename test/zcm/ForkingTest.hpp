#ifndef FORKINGTEST_HPP
#define FORKINGTEST_HPP

#include "cxxtest/TestSuite.h"

#include "zcm/zcm.h"
#include <unistd.h>
#include <sys/wait.h>

#define CHANNEL "TEST_CHANNEL"

static size_t numrecv = 0;
static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    numrecv++;
}

bool sub(zcm_t *zcm)
{
    zcm_subscribe(zcm, CHANNEL, handler, NULL);

    zcm_start(zcm);
    usleep(500000);
    zcm_stop(zcm);

    return numrecv == 1;
}

void pub(zcm_t *zcm)
{
    // Sleep for a moment to give the sub() process time to start
    usleep(100000);

    uint8_t data = 'd';
    zcm_publish(zcm, CHANNEL, &data, 1);
    usleep(200000);
    zcm_publish(zcm, CHANNEL, &data, 1);
    usleep(10000);
}

class ForkingTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testForking() {
        zcm_t *zcm = zcm_create("ipc");

        pid_t pid;
        pid = ::fork();

        if (pid < 0) {
            printf("Fork failed!\n");
            exit(1);
        }

        else if (pid == 0) {
            pub(zcm);
        }

        else {
            bool success = sub(zcm);
            int status;
            ::waitpid(-1, &status, 0);
            TS_ASSERT(success);
        }

        zcm_destroy(zcm);
    }

};

#endif // FORKINGTEST_HPP
