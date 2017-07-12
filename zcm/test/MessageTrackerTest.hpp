#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

#include "cxxtest/TestSuite.h"

#include <zcm/message_tracker.hpp>

using namespace std;

class MessageTrackerTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    struct example_t {
        uint64_t utime;
        int data;
        virtual ~example_t() {}
        int decode(void* data, int start, int max) { return 0; }
        static const char* getTypeName() { return "example_t"; }
    };

    struct data_t {
        uint64_t utime;
        int offset;
        int bufInd;
        virtual ~data_t() {}
    };

    void testFreqStats()
    {
        constexpr size_t numMsgs = 1000;
        zcm::MessageTracker<example_t> mt(nullptr, "", 0.25, numMsgs);
        for (size_t i = 0; i < numMsgs; ++i) {
            example_t tmp = {};
            tmp.utime = i * 1e4;
            mt.newMsg(&tmp, tmp.utime + 1);
            TS_ASSERT_EQUALS(mt.lastMsgHostUtime(), tmp.utime + 1);
        }
        TS_ASSERT_DELTA(mt.getHz(), 100, 1e-5);
        TS_ASSERT_LESS_THAN(mt.getJitterUs(), 1e-5);
    }

    void testGetRange()
    {
        constexpr size_t numMsgs = 1000;
        zcm::Tracker<example_t> mt(0.25, numMsgs);
        for (size_t i = 0; i < numMsgs; ++i) {
            example_t tmp = {};
            tmp.utime = i + 101;
            mt.newMsg(&tmp);
        }

        vector<example_t*> gotRange = mt.getRange(101, 105);
        TS_ASSERT_EQUALS(gotRange.size(), 5);
        uint64_t lastUtime = 0;
        for (auto msg : gotRange) {
            TS_ASSERT(msg->utime >= 101 && msg->utime <= 105);
            TS_ASSERT_LESS_THAN(lastUtime, msg->utime);
            lastUtime = msg->utime;
            delete msg;
        }

        gotRange = mt.getRange(105, 105);
        TS_ASSERT_EQUALS(gotRange.size(), 1);
        for (auto msg : gotRange) {
            TS_ASSERT_EQUALS(msg->utime, 105);
            delete msg;
        }

        gotRange = mt.getRange(110, 105);
        TS_ASSERT_EQUALS(gotRange.size(), 0);

        gotRange = mt.getRange(0, 100);
        TS_ASSERT_EQUALS(gotRange.size(), 0);

        gotRange = mt.getRange(0, 101);
        TS_ASSERT_EQUALS(gotRange.size(), 1);
        for (auto msg : gotRange) {
            TS_ASSERT_EQUALS(msg->utime, 101);
            delete msg;
        }

        gotRange = mt.getRange(1200, 1300);
        TS_ASSERT_EQUALS(gotRange.size(), 0);

        gotRange = mt.getRange(1100, 1300);
        TS_ASSERT_EQUALS(gotRange.size(), 1);
        for (auto msg : gotRange) {
            TS_ASSERT_EQUALS(msg->utime, 1100);
            delete msg;
        }
    }

    void testGetInternalBuf()
    {
        struct data_t {
            uint64_t utime;
            int offset;
            int bufInd;
            int decode(void* data, int start, int max) { return 0; }
            static const char* getTypeName() { return "data_t"; }
            virtual ~data_t() {}
        };

        size_t numMsgs = 10;
        zcm::MessageTracker<data_t> mt(nullptr, "", 0.25, numMsgs);
        for (int i = 0; i < 10; i++) {
            data_t d;
            d.utime = 123456780 + (uint64_t)i;
            d.offset = 100 + i;
            d.bufInd = i;
            mt.newMsg(&d, 0);
        }
        data_t* out = mt.get((uint64_t)123456785);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        delete out;
        out = mt.get((uint64_t)123456790);
        TS_ASSERT_EQUALS(out->bufInd, 9);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 9);
        delete out;
    }

    // Same as previous test, just using the Tracker class instead of message trackers
    void testGetTrackerUsingInternalBuf()
    {
        size_t numMsgs = 100;
        zcm::Tracker<data_t> mt(0.25, numMsgs);
        for (int i = 0; i < (int) numMsgs; i++) {
            data_t d;
            d.utime = 1234567810 + (uint64_t)i;
            d.offset = 100 + i;
            d.bufInd = i;
            mt.newMsg(&d);
        }
        data_t* out = mt.get((uint64_t)1234567815);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        delete out;
        out = mt.get((uint64_t)1234567950);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, (uint64_t) 99);
        delete out;
        out = mt.get((uint64_t)1234567890);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->utime, (uint64_t)1234567890);
        delete out;
    }

    void testGetTrackerUsingProvidedBuf()
    {
        size_t numMsgs = 10;
        zcm::Tracker<data_t> mt(0.25, numMsgs);
        std::vector<data_t*> buf(10);
        data_t d;
        for (int i = 0; i < 10; i++) {
            d.utime = 1234567810 + (uint64_t)i;
            d.offset = 100 + i;
            d.bufInd = i;
            data_t* tmp = new data_t(d);
            buf[i] = tmp;
        }

        data_t* out = mt.get((uint64_t)1234567815, buf.begin(), buf.end());
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        delete out;
        out = mt.get((uint64_t)1234567840, buf.begin(), buf.end());
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 9);
        delete out;
        out = mt.get((uint64_t)1234567813, std::next(buf.begin(), 5), buf.end());
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        delete out;
        // free the dynamically allocated memory
        for (int i = 0; i < 10; i++) {
            delete buf[i];
         }
    }

    void testNoUtime()
    {
        struct test_t {
            int test;
            virtual ~test_t() {}
        };
        // This would not compile if it were broken
        zcm::Tracker<test_t> mt(0.25, 1);
    }

    void testSynchronizedMessageTracker()
    {
        bool pairDetected = false;

        zcm::SynchronizedMessageTracker
            <example_t, example_t,
             zcm::MessageTracker<example_t>,
             zcm::MessageTracker<example_t>>::callback cb =
        [&pairDetected] (example_t *a, example_t *b, void *usr) {
            TS_ASSERT(a); TS_ASSERT(b);
            cout << endl << "Pair detected. Utime a: " << a->utime << endl
                         << "               Utime b: " << b->utime << endl;
            pairDetected = true;
        };

        zcm::ZCM zcmL;

        zcm::SynchronizedMessageTracker
            <example_t, example_t,
             zcm::MessageTracker<example_t>,
             zcm::MessageTracker<example_t>> smt(&zcmL, 10,
                                                 "", 1, 10,
                                                 "", 1, 10,
                                                 cb);
        example_t e1 = {}; e1.utime = 1; e1.data = 1;
        example_t e2 = {}; e2.utime = 2; e2.data = 2;
        example_t e3 = {}; e3.utime = 3; e3.data = 3;
        example_t e5 = {}; e5.utime = 5; e5.data = 5;

        // Message type 2
        smt.t2.newMsg(&e1, 0);
        smt.t2.newMsg(&e2, 0);

        // Message type 1
        smt.t1.newMsg(&e3, 0);

        // Message type 2
        smt.t2.newMsg(&e5, 0);

        TS_ASSERT(pairDetected);
    }
};
