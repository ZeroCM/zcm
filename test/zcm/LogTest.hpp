#ifndef LOGTEST_HPP
#define LOGTEST_HPP


#include "zcm/eventlog.h"
#include "cxxtest/TestSuite.h"

using namespace std;

class LogTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testLog() {
        std::string testChannel = "chan";
        std::string testData    = "test data";
        zcm_eventlog_event_t event;
        event.eventnum   = 0;
        event.timestamp  = 1;
        event.channellen = testChannel.length();
        event.channel    = (char*) testChannel.c_str();
        event.datalen    = testData.length();
        event.data       = (uint8_t*) testData.c_str();

        zcm_eventlog_t *l = zcm_eventlog_create("testlog.log", "w");
        TSM_ASSERT("Failed to open log for writing", l);
        for (size_t i = 0; i < 100; ++i) {
            TSM_ASSERT_EQUALS("Unable to write log event to log", zcm_eventlog_write_event(l, &event), 0);
            event.eventnum++;
            event.timestamp++;
        }
        zcm_eventlog_destroy(l);

        l = zcm_eventlog_create("testlog.log", "r");
        TSM_ASSERT("Failed to read in log", l);

        // Start from end of log and mess up the sync. then ensure everything still works
        fseeko(zcm_eventlog_get_fileptr(l),  -1, SEEK_END);

        for (size_t i = 0; i < 100; ++i) {
            event.eventnum--;
            event.timestamp--;
            zcm_eventlog_event_t *le = zcm_eventlog_read_prev_event(l);
            TSM_ASSERT("Failed to read prev log event out of log", le);
            TSM_ASSERT_EQUALS("Incorrect eventnum inside of prev event", le->eventnum, event.eventnum);
            TSM_ASSERT_EQUALS("Incorrect timestamp inside of prev event", le->timestamp, event.timestamp);
            TSM_ASSERT_EQUALS("Incorrect channellen inside of prev event", le->channellen, event.channellen);
            TSM_ASSERT_EQUALS("Incorrect data inside of prev event", strncmp((const char*)le->channel, testChannel.c_str(), le->channellen), 0);
            TSM_ASSERT_EQUALS("Incorrect channellen inside of prev event", le->datalen, event.datalen);
            TSM_ASSERT_EQUALS("Incorrect data inside of prev event", strncmp((const char*)le->data, testData.c_str(), le->datalen), 0);
            zcm_eventlog_free_event(le);
        }

        TSM_ASSERT("Requesting event before first event didn't return NULL", !zcm_eventlog_read_prev_event(l));

        for (size_t i = 0; i < 100; ++i) {
            zcm_eventlog_event_t *le = zcm_eventlog_read_next_event(l);
            TSM_ASSERT("Failed to read next log event out of log", le);
            TSM_ASSERT_EQUALS("Incorrect eventnum inside of next event", le->eventnum, event.eventnum);
            TSM_ASSERT_EQUALS("Incorrect timestamp inside of next event", le->timestamp, event.timestamp);
            TSM_ASSERT_EQUALS("Incorrect channellen inside of next event", le->channellen, event.channellen);
            TSM_ASSERT_EQUALS("Incorrect data inside of next event", strncmp((const char*)le->channel, testChannel.c_str(), le->channellen), 0);
            TSM_ASSERT_EQUALS("Incorrect channellen inside of next event", le->datalen, event.datalen);
            TSM_ASSERT_EQUALS("Incorrect data inside of next event", strncmp((const char*)le->data, testData.c_str(), le->datalen), 0);
            zcm_eventlog_free_event(le);
            event.eventnum++;
            event.timestamp++;
        }

        TSM_ASSERT("Requesting event after last event didn't return NULL", !zcm_eventlog_read_next_event(l));

        fseeko(zcm_eventlog_get_fileptr(l), 0, SEEK_SET);
        for (size_t i = 0; i < 10; ++i) {
            zcm_eventlog_event_t *le = zcm_eventlog_read_next_event(l);
            zcm_eventlog_free_event(le);
        }
        off_t offset = ftello(zcm_eventlog_get_fileptr(l));
        fseeko(zcm_eventlog_get_fileptr(l), 0, SEEK_SET);

        zcm_eventlog_event_t *le = zcm_eventlog_read_event_at_offset(l, offset);
        TSM_ASSERT("Failed to read offset log event out of log", le);
        TSM_ASSERT_EQUALS("Incorrect eventnum inside of offset event", le->eventnum, 10);
        TSM_ASSERT_EQUALS("Incorrect timestamp inside of offset event", le->timestamp, 11);
        TSM_ASSERT_EQUALS("Incorrect channellen inside of offset event", le->channellen, event.channellen);
        TSM_ASSERT_EQUALS("Incorrect data inside of offset event", strncmp((const char*)le->channel, testChannel.c_str(), le->channellen), 0);
        TSM_ASSERT_EQUALS("Incorrect channellen inside of offset event", le->datalen, event.datalen);
        TSM_ASSERT_EQUALS("Incorrect data inside of offset event", strncmp((const char*)le->data, testData.c_str(), le->datalen), 0)
        zcm_eventlog_free_event(le);

        zcm_eventlog_destroy(l);

        int ret = system("rm testlog.log");
        (void) ret;
    }
};

#endif
