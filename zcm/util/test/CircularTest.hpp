#pragma once

#include "cxxtest/TestSuite.h"

#include "Circular.hpp"

using namespace std;

class CircularTest: public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testConstruct()
    {
        Circular<int> c(10);
    }

    void testPushRemove()
    {
        Circular<int> c(10);

        TS_ASSERT_EQUALS(c.size(), 0);
        TS_ASSERT(c.isEmpty());
        TS_ASSERT(!c.isFull());

        for (size_t i = 0; i < 9; ++i) {
            c.pushBack(new int(i));
            TS_ASSERT_EQUALS(*c.back(), i);
            TS_ASSERT_EQUALS(c.size(), i + 1);
            TS_ASSERT(!c.isEmpty());
            TS_ASSERT(!c.isFull());
        }

        c.pushBack(new int(9));
        TS_ASSERT_EQUALS(c.size(), 10);
        TS_ASSERT(!c.isEmpty());
        TS_ASSERT(c.isFull());


        for (size_t i = 0; i < 10; ++i)
            TS_ASSERT_EQUALS(*c[i], i);


        for (size_t i = 9; i > 0; --i) {
            TS_ASSERT_EQUALS(*c.front(), 9 - i);
            TS_ASSERT_EQUALS(c.size(), i + 1);
            TS_ASSERT(!c.isEmpty());
            c.removeFront();
            TS_ASSERT(!c.isFull());
        }

        c.removeFront();
        TS_ASSERT_EQUALS(c.size(), 0);
        TS_ASSERT(c.isEmpty());
        TS_ASSERT(!c.isFull());

    }

    void testPushPop()
    {
        Circular<int> c(10);

        c.pushFront(new int(1));

        TS_ASSERT_EQUALS(*c.back(), *c.front());
        TS_ASSERT_EQUALS(*c.back(), 1);

        int* i = c.back();
        c.popBack();
        delete i;

        TS_ASSERT(c.isEmpty());
    }
};
