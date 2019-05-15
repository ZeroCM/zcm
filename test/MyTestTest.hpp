#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

#include "cxxtest/TestSuite.h"

#include <zcm/message_tracker.hpp>

using namespace std;

class MyTestTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testMyFirstTest() {
        TS_ASSERT(1);
    }

    void testMySecondTest() {
        TS_ASSERT(1);
    }
};
