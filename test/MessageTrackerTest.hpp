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
            mt.newMsg(tmp, tmp.utime + 1);
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
            mt.newMsg(tmp);
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
            mt.newMsg(d, 0);
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
            mt.newMsg(d);
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

    void testUtimeFunc()
    {
        struct test_t {
            uint64_t test;
            test_t(uint64_t test = 0) : test(test) {}
            uint64_t getUtime() const { return test; }
            virtual ~test_t() {}
        };

        zcm::Tracker<test_t> mt(0.25, 3);

        test_t tst1(100);
        TS_ASSERT_EQUALS(mt.newMsg(tst1, 1), 100);

        test_t tst2(200);
        TS_ASSERT_EQUALS(mt.newMsg(tst2, 2), 200);

        test_t tst3(300);
        TS_ASSERT_EQUALS(mt.newMsg(tst3, 3), 300);

        auto *msg = mt.get(200);
        TS_ASSERT_EQUALS(msg->getUtime(), 200);
        delete msg;
    }

    void testUtimeField()
    {
        struct test_t {
            uint64_t utime;
            test_t(uint64_t utime = 0) : utime(utime) {}
            virtual ~test_t() {}
        };

        zcm::Tracker<test_t> mt(0.25, 3);

        test_t tst1(100);
        TS_ASSERT_EQUALS(mt.newMsg(tst1, 1), 100);

        test_t tst2(200);
        TS_ASSERT_EQUALS(mt.newMsg(tst2, 2), 200);

        test_t tst3(300);
        TS_ASSERT_EQUALS(mt.newMsg(tst3, 3), 300);

        auto *msg = mt.get(200);
        TS_ASSERT_EQUALS(msg->utime, 200);
        delete msg;
    }

    void testNoUtime()
    {
        struct test_t {
            int test;
            test_t(int test = 0) : test(test) {}
            virtual ~test_t() {}
        };

        zcm::Tracker<test_t> mt(0.25, 3);

        test_t tst1(10);
        TS_ASSERT_EQUALS(mt.newMsg(tst1, 1), 1);

        test_t tst2(20);
        TS_ASSERT_EQUALS(mt.newMsg(tst2, 2), 2);

        test_t tst3(30);
        TS_ASSERT_EQUALS(mt.newMsg(tst3, 3), 3);

        auto *msg = mt.get(2);
        TS_ASSERT_EQUALS(msg->test, 20);
        delete msg;
    }

    //void testNoUtime()
    //{
        //struct test_t {
            //int test;
            //virtual ~test_t() {}
        //};
        //// This should not compile
        //zcm::Tracker<test_t> mt(0.25, 1);
    //}

    void testSynchronizedMessageDispatcher()
    {
        int pairDetected = 0;

        example_t e1 = {}; e1.utime = 1; e1.data = 1;
        example_t e2 = {}; e2.utime = 2; e2.data = 2;
        example_t e3 = {}; e3.utime = 3; e3.data = 3;
        example_t e4 = {}; e4.utime = 4; e4.data = 4;
        example_t e5 = {}; e5.utime = 5; e5.data = 5;
        example_t e6 = {}; e6.utime = 6; e6.data = 6;
        example_t e7 = {}; e7.utime = 7; e7.data = 7;

        class tracker : public zcm::MessageTracker<example_t> {
          public:
            tracker(zcm::ZCM* zcmLocal, std::string channel,
                    double maxTimeErr, size_t maxMsgs) :
                zcm::Tracker<example_t>(maxTimeErr, maxMsgs),
                zcm::MessageTracker<example_t>(zcmLocal, channel, maxTimeErr, maxMsgs) {}

            example_t* interpolate(uint64_t utimeTarget,
                                   const example_t* A, uint64_t utimeA,
                                   const example_t* B, uint64_t utimeB) const override
            {
                TS_ASSERT_EQUALS(utimeA, 2); TS_ASSERT_EQUALS(A->utime, 2);
                TS_ASSERT_EQUALS(utimeB, 5); TS_ASSERT_EQUALS(B->utime, 5);
                example_t* ret = new example_t();
                ret->utime = 10;
                return ret;
            }
        };

        zcm::SynchronizedMessageDispatcher <zcm::MessageTracker<example_t>, tracker>::callback cb =
        [&] (example_t *a, example_t *b, void *usr) {
            TS_ASSERT(a); TS_ASSERT(b);
            if (pairDetected == 0) {
                TS_ASSERT_EQUALS(a->utime, 3);
            } else {
                TS_ASSERT_EQUALS(a->utime, 4);
            }
            TS_ASSERT_EQUALS(b->utime, 10);
            ++pairDetected;
            delete a; delete b;
        };

        zcm::ZCM zcmL;

        zcm::SynchronizedMessageDispatcher<zcm::MessageTracker<example_t>, tracker>
            smt(&zcmL,
                "",    10,
                "", 1, 10,
                cb);

        std::stringstream ss;

        // Message type 2
        ss << "New message b: " << e1.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t2.handle(&e1);
        ss << "New message b: " << e2.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t2.handle(&e2);

        // Message type 1
        ss << "New message a: " << e3.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t1.handle(&e3);

        // Message type 2
        ss << "New message b: " << e5.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t2.handle(&e5);
        ss << "New message b: " << e6.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t2.handle(&e6);

        // Message type 1
        ss << "New message a: " << e4.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t1.handle(&e4);
        ss << "New message b: " << e7.utime; TS_TRACE(ss.str()); ss.str("");
        smt.t2.handle(&e7);

        TS_ASSERT_EQUALS(pairDetected, 2);
    }

    void testSynchronizedMessageWithModifiedData()
    {
        int pairDetected = 0;

        example_t e1 = {}; e1.utime = 1; e1.data = 0;
        example_t e2 = {}; e2.utime = 2; e2.data = 2;

        class tracker : public zcm::MessageTracker<example_t> {
          public:
            tracker(zcm::ZCM* zcmLocal, std::string channel,
                    double maxTimeErr, size_t maxMsgs) :
                zcm::Tracker<example_t>(maxTimeErr, maxMsgs),
                zcm::MessageTracker<example_t>(zcmLocal, channel, maxTimeErr, maxMsgs) {}

            uint64_t newMsg(const example_t& msg, uint64_t hostUtime = UINT64_MAX) override
            {
                example_t newMsg = msg;
                newMsg.data = 1;
                return Tracker<example_t>::newMsg(newMsg, hostUtime);
            }
        };

        zcm::SynchronizedMessageDispatcher <tracker, zcm::MessageTracker<example_t>>::callback
            cb = [&] (example_t *a, example_t *b, void *usr) {
                     TS_ASSERT(a); TS_ASSERT(b);
                     TS_ASSERT_EQUALS(a->data, 1);
                     ++pairDetected;
                     delete a; delete b;
                 };

        zcm::ZCM zcmL;

        zcm::SynchronizedMessageDispatcher<tracker, zcm::MessageTracker<example_t>>
            smt(&zcmL,
                "",    10,
                "", 1, 10,
                cb);

        std::stringstream ss;

        // Message type 2
        smt.t2.handle(&e2);
        smt.t1.handle(&e1);

        TS_ASSERT_EQUALS(pairDetected, 1);
    }
};
