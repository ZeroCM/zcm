#pragma once

#include <iostream>

#include "cxxtest/TestSuite.h"

#include "Filter.hpp"

class FilterTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testEndResult()
    {
        zcm::Filter f(1, 0.8);
        for (double i = 0; i < 2000; i++) f(10, 0.01);
        TS_ASSERT_DELTA(f[f.FilterMode::LOW_PASS],  10, 1e-5);
        TS_ASSERT_DELTA(f[f.FilterMode::BAND_PASS],  0, 1e-5);
        TS_ASSERT_DELTA(f[f.FilterMode::HIGH_PASS],  0, 1e-5);
    }

    void testConvergenceTimeEst()
    {
        std::cout << zcm::Filter::convergenceTimeToNatFreq(10, 0.8) << std::endl;
    }
};