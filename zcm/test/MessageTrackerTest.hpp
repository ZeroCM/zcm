#pragma once

#include "cxxtest/TestSuite.h"

#include <zcm/zcm-cpp.hpp>

#include "message_tracker.hpp"

#include "test/types/example_t.hpp"

using namespace std;

class MessageTrackerTest: public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testConstruct()
    {
        zcm::ZCM zcmLocal;
        MessageTracker<example_t> mt(&zcmLocal, "EXAMPLE");
        zcmLocal.start();
        auto tmp = mt.get();
        zcmLocal.stop();
        delete tmp;
    }
};
