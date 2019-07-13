#ifndef STRESSTEST_HPP
#define STRESSTEST_HPP

#include "cxxtest/TestSuite.h"

#include <zcm/zcm.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define CHANNEL "HIGHRATE_TEST"
#define NUM_MESSAGES_PER_TEST 1000

static size_t recv_count = 0;
static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    recv_count++;
}

using namespace std;

class StressTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testFastMessagesOnSingleChannel() {
        TS_SKIP("All transports still have undetectable incoming package loss!");
        return;
        for(const char* transport_url : {"ipc", "inproc", "udpm://239.255.76.67:7667?ttl=0"}) {
            for(const int num_bytes : {10, 100, 1000, 10000, 100000}) {
                for(const int publish_period_us : {0, 10, 100, 1000}) {
                    uint8_t *data = (uint8_t*) malloc(num_bytes);
                    memset(data, 0, num_bytes);

                    zcm_t *zcm = zcm_create(transport_url);
                    TS_ASSERT(zcm);

                    zcm_sub_t* subscription = zcm_subscribe(zcm, CHANNEL, handler, NULL);
                    TS_ASSERT(subscription);
                    zcm_start(zcm);

                    int num_messages_sent_sucessfully = 0;
                    for (size_t i = 0; i < NUM_MESSAGES_PER_TEST; i++) {
                        if(zcm_publish(zcm, CHANNEL, data, num_bytes) == ZCM_EOK) {
                            num_messages_sent_sucessfully++;
                        }
                        usleep(publish_period_us);
                    }

                    usleep(100000);
                    zcm_unsubscribe(zcm, subscription);
                    zcm_stop(zcm);
                    zcm_destroy(zcm);
                    free(data);

                    TS_ASSERT_EQUALS(recv_count, num_messages_sent_sucessfully);
                    recv_count = 0;
                }
            }
        }
    }

};


#endif // STRESSTEST_HPP
