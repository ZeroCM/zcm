#pragma once



#include "cxxtest/TestSuite.h"

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
