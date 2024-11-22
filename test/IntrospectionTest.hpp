#pragma once

#include <unistd.h>

#include "cxxtest/TestSuite.h"

#include "zcm/zcm-cpp.hpp"
#include "types/example_t.hpp"
#include "types/recursive_t.hpp"
#include "tools/cpp/util/Introspection.hpp"

using namespace std;

class IntrospectionTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testSimple()
    {
        example_t msg;
        msg.num_ranges = 4;
        msg.ranges.resize(msg.num_ranges);
        zcm::ZCM zcmL;
        zcmL.publish("TEST", &msg);
        zcmL.flush();
        ::usleep(5e5);
        zcmL.publish("TEST", &msg);
        zcmL.flush();
    }

    void testRecursive()
    {
        recursive_t msg;

        auto makeChild = [](recursive_t *parent){
            parent->n_children = 1;
            parent->children.resize(parent->n_children);
            recursive_t* child = &parent->children[0];
            return child;
        };

        recursive_t *child = makeChild(&msg);
        child = makeChild(child);

        child->text = "test";

        zcm::ZCM zcmL;
        zcmL.publish("TEST", &msg);
        zcmL.flush();
        ::usleep(5e5);
        zcmL.publish("TEST", &msg);
        zcmL.flush();

        zcm::Introspection::processType("TEST", );
    }
};
