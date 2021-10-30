#pragma once

#include "cxxtest/TestSuite.h"

#include "types/bitfield_t.h"

using namespace std;

class BitfieldTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    void testEncode()
    {
        bitfield_t b1 = {};
        b1.field1 = 3;
        b1.field2[0][0] = 1;
        b1.field2[0][1] = 0;
        b1.field2[0][2] = 1;
        b1.field2[1][0] = 0;
        b1.field2[1][1] = 1;
        b1.field2[1][2] = 0;
        b1.field4 = 7;
        TS_ASSERT_EQUALS(b1.field1, 3);
        TS_ASSERT_EQUALS(__bitfield_t_encoded_array_size(&b1, 1), 6);
        uint8_t buf[6];
        TS_ASSERT_EQUALS(__bitfield_t_encode_array(&buf, 0, 6, &b1, 1), 6);

        bitfield_t b2;
        TS_ASSERT_EQUALS(__bitfield_t_decode_array(buf, 0, 6, &b2, 1), 6);

        TS_ASSERT_EQUALS(b1.field1, b2.field1);
        TS_ASSERT_EQUALS(b1.field2[0][0], b2.field2[0][0]);
        TS_ASSERT_EQUALS(b1.field2[0][1], b2.field2[0][1]);
        TS_ASSERT_EQUALS(b1.field2[0][2], b2.field2[0][2]);
        TS_ASSERT_EQUALS(b1.field2[1][0], b2.field2[1][0]);
        TS_ASSERT_EQUALS(b1.field2[1][1], b2.field2[1][1]);
        TS_ASSERT_EQUALS(b1.field2[1][2], b2.field2[1][2]);
        TS_ASSERT_EQUALS(b1.field3, b2.field3);
        TS_ASSERT_EQUALS(b1.field4, b2.field4);
        TS_ASSERT_EQUALS(b1.field5, b2.field5);
        TS_ASSERT_EQUALS(b1.field6, b2.field6);
        TS_ASSERT_EQUALS(b1.field7_dim1, b2.field7_dim1);
        TS_ASSERT_EQUALS(b1.field7_dim2, b2.field7_dim2);
    }
};
