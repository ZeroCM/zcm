#include "zcm/eventlog.h"
#include <assert.h>
#include <string.h>
#include <string>

// RRR (Tom) would like to see this test write multiple events (e.g. just change
//     eventnum) and read multiple events forward and back) -- at least do something
//     to confirm that read_prev actually does anything besides read the first e
//     event in the log.  I think the code would actually look pretty nice with a
//     couple for loops forward and backward.
int main(int argc, const char *argv[])
{
    zcm_eventlog_t *l = zcm_eventlog_create("testlog.log", "w");
    assert(l && "Failed to open log for writing");
    std::string testChannel = "chan";
    std::string testData    = "test data";
    zcm_eventlog_event_t event;
    event.eventnum   = 0;
    event.timestamp  = 1;
    event.channellen = testChannel.length();
    event.channel    = (char*) testChannel.c_str();
    event.datalen    = testData.length();
    event.data       = (void*) testData.c_str();
    assert(zcm_eventlog_write_event(l, &event) == 0 &&
           "Unable to write log event to log");
    zcm_eventlog_destroy(l);

    l = zcm_eventlog_create("testlog.log", "r");
    assert(l && "Failed to read in log");

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

    le = zcm_eventlog_read_prev_event(l);
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

    zcm_eventlog_destroy(l);

    int ret = system("rm testlog.log");
    (void) ret;
    return 0;
}
