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
        int decode(void* data, int start, int max) { return 0; }
        static const char* getTypeName() { return "example_t"; }
    };

    struct data_t {
            uint64_t utime;
            int offset;
            int bufInd;
        };

    void testFreqStats()
    {
        constexpr size_t numMsgs = 1000;
        zcm::MessageTracker<example_t> mt(nullptr, "", 0.25, numMsgs);
        for (size_t i = 0; i < 1000; ++i) {
            example_t tmp;
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
        zcm::MessageTracker<example_t> mt(nullptr, "", 0.0001, numMsgs);
        for (size_t i = 0; i < 1000; ++i) {
            example_t tmp;
            tmp.utime = i + 101;
            mt.newMsg(&tmp);
        }

        vector<example_t*> gotRange = mt.getRange(101, 105);
        TS_ASSERT_EQUALS(gotRange.size(), 5);
        for (auto msg : gotRange) {
            TS_ASSERT(msg->utime >= 101 && msg->utime <= 105);
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

        gotRange = mt.getRange(0, 1);
        TS_ASSERT_EQUALS(gotRange.size(), 1);
        for (auto msg : gotRange) {
            TS_ASSERT_EQUALS(msg->utime, 101);
            delete msg;
        }

        gotRange = mt.getRange(1200, 1300);
        TS_ASSERT_EQUALS(gotRange.size(), 1);
        for (auto msg : gotRange) {
            TS_ASSERT_EQUALS(msg->utime, 1100);
            delete msg;
        }

        gotRange = mt.getRange(1201, 1300);
        TS_ASSERT_EQUALS(gotRange.size(), 0);

        gotRange = mt.getRange(0, 0);
        TS_ASSERT_EQUALS(gotRange.size(), 0);

        gotRange = mt.getRange(100, 100);
        TS_ASSERT_EQUALS(gotRange.size(), 1);
        for (auto msg : gotRange) {
            TS_ASSERT_EQUALS(msg->utime, 101);
            delete msg;
        }

        gotRange = mt.getRange(1102, 1205);
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
        };

        size_t numMsgs = 10;
        zcm::MessageTracker<data_t> mt(nullptr, "", 0.25, numMsgs);
        for (int i = 0; i < 10; i++) {
            data_t d = {123456780 + (uint64_t)i, 100 + i, i};
            mt.newMsg(&d, 0);
        }
        data_t* out = mt.get((uint64_t)123456785);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        out = mt.get((uint64_t)123456790);
        TS_ASSERT_EQUALS(out->bufInd, 9);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 9);

    }

    void testGetTrackerUsingInternalBuf()
    {
        size_t numMsgs = 100;
        zcm::Tracker<data_t> mt(0.25, numMsgs);
        for (int i = 0; i < (int) numMsgs; i++) {
            data_t d = {1234567810  + (uint64_t)i, 100 + i, i};
            mt.newMsg(&d);
        }
        data_t* out = mt.get((uint64_t)1234567815);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        out = mt.get((uint64_t)1234567950);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, (uint64_t) 99);
        out = mt.get((uint64_t)1234567890);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->utime, (uint64_t)1234567890);

    }

    void testGetTrackerUsingProvidedBuf()
    {

        size_t numMsgs = 10;
        zcm::Tracker<data_t> mt(0.25, numMsgs);
        std::vector<data_t*> buf(10);
        data_t d;
        for (int i = 0; i < 10; i++) {
            d.utime = 1234567810  + (uint64_t)i;
            d.bufInd = i;
            data_t* tmp = new data_t(d);
            buf[i] = tmp;
        }

        std::mutex bufLock;
        std::unique_lock<std::mutex> lk(bufLock);
        data_t* out = mt.get((uint64_t)1234567815, buf.begin(), buf.end(), lk);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);
        out = mt.get((uint64_t)1234567840, buf.begin(), buf.end(), lk);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 9);
        out = mt.get((uint64_t)1234567813, std::next(buf.begin(), 5), buf.end(), lk);
        TS_ASSERT(out != nullptr);
        if (out!= nullptr)
            TS_ASSERT_EQUALS(out->bufInd, 5);

        // free the dynamically allocated memory
        for (int i = 0; i < 10; i++) {
            delete buf[i];
         }
    }
};
