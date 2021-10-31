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
        bitfield_t b = {};
        b.field1 = 3;
        b.field2[0][0] = 1;
        b.field2[0][1] = 0;
        b.field2[0][2] = 1;
        b.field2[0][3] = 0;
        b.field2[1][0] = 1;
        b.field2[1][1] = 0;
        b.field2[1][2] = 1;
        b.field2[1][3] = 0;
        b.field3 = 0xf;
        b.field4 = 5;
        b.field5 = 7;
        b.field9 = 1 << 27;
        b.field10 = ((uint64_t)1 << 52) | 1;
        TS_ASSERT_EQUALS(__bitfield_t_encoded_array_size(&b, 1), 18);
        uint8_t buf[18];
        TS_ASSERT_EQUALS(__bitfield_t_encode_array(&buf, 0, 18, &b, 1), 18);

        uint8_t enc[18] = {
            0b11101010, // fields 1, 2, 3, and 4
            0b10111110, // fields 1, 2, 3, and 4
            0b10000000, // fields 1, 2, 3, and 4
            7,          // field5
            0,          // fields 6, 6, and 8_dim1
            0,          // fields 6, 7, and 8_dim1
            0,          // field8_dim2
            0b10000000, // field9
            0,          // field9
            0,          // field9
            0b00000010, // fields 9 and 10
            0,          // field10
            0,          // field10
            0,          // field10
            0,          // field10
            0,          // field10
            0,          // field10
            0b00100000  // field10
        };
        TS_ASSERT_EQUALS(buf[0], enc[0]);
        TS_ASSERT_EQUALS(buf[1], enc[1]);
        TS_ASSERT_EQUALS(buf[2], enc[2]);
        TS_ASSERT_EQUALS(buf[3], enc[3]);
        TS_ASSERT_EQUALS(buf[4], enc[4]);
        TS_ASSERT_EQUALS(buf[5], enc[5]);
        TS_ASSERT_EQUALS(buf[6], enc[6]);
        TS_ASSERT_EQUALS(buf[7], enc[7]);
        TS_ASSERT_EQUALS(buf[8], enc[8]);
        TS_ASSERT_EQUALS(buf[9], enc[9]);
        TS_ASSERT_EQUALS(buf[10], enc[10]);
        TS_ASSERT_EQUALS(buf[11], enc[11]);
        TS_ASSERT_EQUALS(buf[12], enc[12]);
        TS_ASSERT_EQUALS(buf[13], enc[13]);
        TS_ASSERT_EQUALS(buf[14], enc[14]);
        TS_ASSERT_EQUALS(buf[15], enc[15]);
        TS_ASSERT_EQUALS(buf[16], enc[16]);
        TS_ASSERT_EQUALS(buf[17], enc[17]);
    }

    void testDecode()
    {
        uint8_t buf[18] = {
            0b11101010, // fields 1, 2, 3, and 4
            0b10111110, // fields 1, 2, 3, and 4
            0b10000000, // fields 1, 2, 3, and 4
            7,          // field5
            0,          // fields 6, 6, and 8_dim1
            0,          // fields 6, 7, and 8_dim1
            0,          // field8_dim2
            0b10000000, // field9
            0,          // field9
            0,          // field9
            0b00000010, // fields 9 and 10
            0,          // field10
            0,          // field10
            0,          // field10
            0,          // field10
            0,          // field10
            0,          // field10
            0b00100000  // field10
        };
        bitfield_t b;
        TS_ASSERT_EQUALS(__bitfield_t_decode_array(buf, 0, 18, &b, 1), 18);

        // Note that a lot of this is testing sign extension
        TS_ASSERT_EQUALS(b.field1, -1);
        TS_ASSERT_EQUALS(b.field2[0][0], -1);
        TS_ASSERT_EQUALS(b.field2[0][1],  0);
        TS_ASSERT_EQUALS(b.field2[0][2], -1);
        TS_ASSERT_EQUALS(b.field2[0][3],  0);
        TS_ASSERT_EQUALS(b.field2[1][0], -1);
        TS_ASSERT_EQUALS(b.field2[1][1],  0);
        TS_ASSERT_EQUALS(b.field2[1][2], -1);
        TS_ASSERT_EQUALS(b.field2[1][3],  0);
        TS_ASSERT_EQUALS(b.field3, -1);
        TS_ASSERT_EQUALS(b.field4, (int8_t)0b11111101);
        TS_ASSERT_EQUALS(b.field5, 7);
        TS_ASSERT_EQUALS(b.field6, 0);
        TS_ASSERT_EQUALS(b.field7, 0);
        TS_ASSERT_EQUALS(b.field8_dim1, 0);
        TS_ASSERT_EQUALS(b.field8_dim2, 0);
        TS_ASSERT_EQUALS(b.field9, 1 << 27);
        TS_ASSERT_EQUALS(b.field10, ((uint64_t)1 << 52) | 1);
    }
};
