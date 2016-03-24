#pragma once

#include "cxxtest/TestSuite.h"

#include "circular.hpp"

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

    void testResize()
    {
        Circular<int> c(10);

        memset(c._data, 0, c.capacity() * sizeof(int*));

        for (int i = 0; i < 10; ++i)
            c.pushBack(new int(i));
        c.removeFront();

        c.resize(5);

        TS_ASSERT_EQUALS(*c.back(), 9);
        TS_ASSERT_EQUALS(*c.front(), 5);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], i + 5);

        c.resize(2);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], i + 8);

        c.removeBack();

        TS_ASSERT_EQUALS(*c.front(), *c.back());

        c.removeBack();
        TS_ASSERT(c.isEmpty());
    }

    void testResizeWrapAround()
    {
        Circular<int> c(10);

        memset(c._data, 0, c.capacity() * sizeof(int*));

        for (int i = 0; i < 10; ++i)
            c.pushBack(new int(i));
        c.removeFront();

        c.resize(5);

        TS_ASSERT_EQUALS(*c.back(), 9);
        TS_ASSERT_EQUALS(*c.front(), 5);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], i + 5);

        c.resize(2);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], i + 8);

        c.removeBack();

        TS_ASSERT_EQUALS(*c.front(), *c.back());

        c.removeBack();
        TS_ASSERT(c.isEmpty());
    }

    void testResizeWrap2()
    {
        Circular<int> c(10);

        memset(c._data, 0, c.capacity() * sizeof(int*));

        c.pushFront(new int(1));
        c.pushFront(new int(0));
        for (int i = 2; i < 10; ++i)
            c.pushBack(new int(i));
        c.removeFront();
        c.removeFront();
        c.pushBack(new int(10));
        c.pushBack(new int(11));

        c.resize(5);

        TS_ASSERT_EQUALS(*c.back(), 11);
        TS_ASSERT_EQUALS(*c.front(), 7);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], i + 7);

        c.resize(2);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], i + 10);

        c.removeBack();

        TS_ASSERT_EQUALS(*c.front(), *c.back());

        c.removeBack();
        TS_ASSERT(c.isEmpty());
    }

    void testResizeTo1()
    {
        Circular<int> c(10);

        memset(c._data, 0, c.capacity() * sizeof(int*));

        c.pushFront(new int(1));
        c.pushFront(new int(0));
        for (int i = 2; i < 10; ++i)
            c.pushBack(new int(i));
        c.removeFront();
        c.removeFront();
        c.pushBack(new int(10));
        c.pushBack(new int(11));

        c.resize(100);
        for (int i = 12; i < 102; ++i)
            c.pushBack(new int(i));
        c.resize(1);

        for (size_t i = 0; i < c.size(); ++i)
            TS_ASSERT_EQUALS(*c[i], 101);

        c.removeBack();
        TS_ASSERT(c.isEmpty());
    }
};
