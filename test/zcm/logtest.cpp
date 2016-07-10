#include "zcm/eventlog.h"
#include <assert.h>
#include <string.h>
#include <string>
#include <iostream>

int main(int argc, const char *argv[])
{
    std::string testChannel = "chan";
    std::string testData    = "test data";
    zcm_eventlog_event_t event;
    event.eventnum   = 0;
    event.timestamp  = 1;
    event.channellen = testChannel.length();
    event.channel    = (char*) testChannel.c_str();
    event.datalen    = testData.length();
    event.data       = (void*) testData.c_str();

    zcm_eventlog_t *l = zcm_eventlog_create("testlog.log", "w");
    assert(l && "Failed to open log for writing");
    for (size_t i = 0; i < 100; ++i) {
        assert(zcm_eventlog_write_event(l, &event) == 0 && "Unable to write log event to log");
        event.eventnum++;
        event.timestamp++;
    }
    zcm_eventlog_destroy(l);

    l = zcm_eventlog_create("testlog.log", "r");
    assert(l && "Failed to read in log");

    // Start from end of log and mess up the sync. then ensure everything still works
    fseeko(zcm_eventlog_get_fileptr(l),  -1, SEEK_END);

    for (size_t i = 0; i < 100; ++i) {
        event.eventnum--;
        event.timestamp--;
        zcm_eventlog_event_t *le = zcm_eventlog_read_prev_event(l);
        assert(le && "Failed to read prev log event out of log");
        assert(le->eventnum == event.eventnum && "Incorrect eventnum inside of prev event");
        assert(le->timestamp == event.timestamp && "Incorrect timestamp inside of prev event");
        assert(le->channellen == event.channellen && "Incorrect channellen inside of prev event");
        assert(strncmp((const char*)le->channel, testChannel.c_str(), le->channellen) == 0 &&
               "Incorrect data inside of prev event");
        assert(le->datalen = event.datalen && "Incorrect channellen inside of prev event");
        assert(strncmp((const char*)le->data, testData.c_str(), le->datalen) == 0 &&
               "Incorrect data inside of prev event");
        zcm_eventlog_free_event(le);
    }

    assert(zcm_eventlog_read_prev_event(l) == NULL &&
           "Requesting event before first event didn't return NULL");

    for (size_t i = 0; i < 100; ++i) {
        zcm_eventlog_event_t *le = zcm_eventlog_read_next_event(l);
        assert(le && "Failed to read next log event out of log");
        assert(le->eventnum == event.eventnum && "Incorrect eventnum inside of next event");
        assert(le->timestamp == event.timestamp && "Incorrect timestamp inside of next event");
        assert(le->channellen == event.channellen && "Incorrect channellen inside of next event");
        assert(strncmp((const char*)le->channel, testChannel.c_str(), le->channellen) == 0 &&
               "Incorrect data inside of next event");
        assert(le->datalen = event.datalen && "Incorrect channellen inside of next event");
        assert(strncmp((const char*)le->data, testData.c_str(), le->datalen) == 0 &&
               "Incorrect data inside of next event");
        zcm_eventlog_free_event(le);
        event.eventnum++;
        event.timestamp++;
    }

    assert(zcm_eventlog_read_next_event(l) == NULL &&
           "Requesting event after last event didn't return NULL");

    zcm_eventlog_destroy(l);

    int ret = system("rm testlog.log");
    (void) ret;

    return 0;
}
