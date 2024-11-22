#pragma once

#include <iostream>
#include <unistd.h>

#include "cxxtest/TestSuite.h"

#include "zcm/zcm-cpp.hpp"
#include "types/example_t.hpp"
#include "tools/cpp/util/Introspection.hpp"
#include "tools/cpp/util/TypeDb.hpp"

extern "C" {
#include "types/recursive_t.h"
}

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
        zcm::TypeDb typeDb("./build/tests/test/types/libtestzcmtypes.so");
        TS_ASSERT(typeDb.good());

        recursive_t msg;
        msg.text = (char*)"";
        msg.n_children = 1;
        msg.children = (recursive_t*)calloc(1, sizeof(recursive_t));
        msg.children[0].text = (char*)"";
        msg.children[0].n_children = 1;
        msg.children[0].children = (recursive_t*)calloc(1, sizeof(recursive_t));
        msg.children[0].children[0].text = (char*)"";
        msg.children[0].children[0].n_children = 1;
        msg.children[0].children[0].children = (recursive_t*)calloc(1, sizeof(recursive_t));
        msg.children[0].children[0].children[0].text = (char*)"test";

        /*
        zcm::ZCM zcmL;
        recursive_t_publish(zcmL.getUnderlyingZCM(), "TEST", &msg);
        zcmL.flush();
        ::usleep(5e5);
        recursive_t_publish(zcmL.getUnderlyingZCM(), "TEST", &msg);
        zcmL.flush();
        */

        auto cb = [](const string& name, zcm_field_type_t type, const void* data, void *usr){
            cout << name << ": ";
            switch (type) {
                case ZCM_FIELD_INT8_T:
                  cout << (int)*((int8_t*)data);
                  break;
                case ZCM_FIELD_INT16_T:
                  cout << (int)*((int16_t*)data);
                  break;
                case ZCM_FIELD_INT32_T:
                  cout << (int)*((int32_t*)data);
                  break;
                case ZCM_FIELD_INT64_T:
                  cout << (long)*((int64_t*)data);
                  break;
                case ZCM_FIELD_BYTE:
                  cout << (int)*((uint8_t*)data);
                  break;
                case ZCM_FIELD_FLOAT:
                  cout << *((float*)data);
                  break;
                case ZCM_FIELD_DOUBLE:
                  cout << *((double*)data);
                  break;
                case ZCM_FIELD_BOOLEAN:
                  cout << (int)*((uint8_t*)data);
                  break;
                case ZCM_FIELD_STRING:
                  cout << string(*((const char**)data));
                  break;
                case ZCM_FIELD_USER_TYPE:
                  assert(false && "Should not be possble");
            }
            cout << endl;
        };

        zcm::Introspection::processType("TEST", *recursive_t_get_type_info(),
                                        &msg, "/", typeDb, cb, nullptr);

        free(msg.children[0].children[0].children);
        free(msg.children[0].children);
        free(msg.children);
    }
};
