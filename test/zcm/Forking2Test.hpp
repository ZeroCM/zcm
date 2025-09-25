#ifndef FORKINGTEST_HPP
#define FORKINGTEST_HPP

#include "cxxtest/TestSuite.h"

#include "zcm/zcm.h"
#include <unistd.h>
#include <sys/wait.h>

#define CHANNEL "TEST_CHANNEL"
#define URL "udpm://239.255.76.67:7667?ttl=0"

static size_t numrecv = 0;
static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    numrecv++;
}

class Forking2Test : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testForking1() {
        zcm_t *zcm = zcm_create(URL);

        pid_t pid;
        pid = ::fork();

        if (pid < 0) {
            printf("%s: Fork failed!\n", __FUNCTION__);
            exit(1);
        }

        else if (pid == 0) {
            pub(zcm);
            exit(0);
        }

        else {
            sub(zcm, 1);
            int status;
            ::waitpid(-1, &status, 0);
        }

        zcm_destroy(zcm);
    }

    void pub(zcm_t *zcm)
    {
        // Sleep for a moment to give the sub() process time to start
        usleep(100000);

        uint8_t data = 'd';
        zcm_publish(zcm, CHANNEL, &data, 1);
        usleep(10000);
    }

    void sub(zcm_t *zcm, size_t expect)
    {
        numrecv = 0;
        zcm_subscribe(zcm, CHANNEL, handler, NULL);

        zcm_start(zcm);
        usleep(500000);
        zcm_stop(zcm);

        TS_ASSERT_EQUALS(numrecv, expect);
    }

};

#endif // FORKINGTEST_HPP
