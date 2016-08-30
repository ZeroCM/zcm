#pragma once

#include <iostream>

#include "cxxtest/TestSuite.h"

#include "Filter.hpp"

// RRR: general comment: are we ok with the tests "infiltrating" the main zcm folder? That means
//      they'll be installed on the system right? Seems like we might want to keep them in the
//      dedicated test folder.

class FilterTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testEndResult()
    {
        zcm::Filter f(1, 0.8);
        for (double i = 0; i < 2000; i++) f(10, 0.01);
        // RRR: shouldn't need "f." here
        TS_ASSERT_DELTA(f[f.FilterMode::LOW_PASS],  10, 1e-5);
        TS_ASSERT_DELTA(f[f.FilterMode::BAND_PASS],  0, 1e-5);
        TS_ASSERT_DELTA(f[f.FilterMode::HIGH_PASS],  0, 1e-5);
        // RRR: obviously a bit harder to test band and highpass, but might be nice to throw a
        //      todo in here
    }

    void testConvergenceTimeEst()
    {
        TS_ASSERT_DELTA(zcm::Filter::convergenceTimeToNatFreq(10, 0.8), 0.24848, 1e-5);
    }
};
