#pragma once

#include "cxxtest/TestSuite.h"

#include "types/bitfield_t.h"

namespace cpp {
#include "types/bitfield_t.hpp"
}

using namespace std;

class BitfieldTest : public CxxTest::TestSuite
{
  public:
    void setUp() override {}
    void tearDown() override {}

    static constexpr size_t expectEncSize = 35;
    uint8_t enc[expectEncSize] = {
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
        0b00111001, // fields 10, 11 and 12
        0b01001110, // field12
        0b01011101, // field12
        0b11000001, // field12
        0b01001110, // field12
        0b01011101, // field12
        0b11000001, // field12
        0b01001110, // field12
        0b01011101, // field12
        0b11000000, // field12
        0,          // field13
        0b00001000, // fields 14 and 15
        0b10000000, // fields 15 and 16
        0b10000000, // field16
        0,          // field17
        0b00001000, // fields 18 and 19
        0b10000000, // fields 19 and 20
        0b10000000  // field20
    };

    void testCEncode()
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
        b.field11 = 3;
        b.field12[0][0][0][0] = 1;
        b.field12[0][0][0][1] = 2;
        b.field12[0][0][1][0] = 3;
        b.field12[0][0][1][1] = 4;
        b.field12[0][1][0][0] = 5;
        b.field12[0][1][0][1] = 6;
        b.field12[0][1][1][0] = 7;
        b.field12[0][1][1][1] = 0;
        b.field12[1][0][0][0] = 1;
        b.field12[1][0][0][1] = 2;
        b.field12[1][0][1][0] = 3;
        b.field12[1][0][1][1] = 4;
        b.field12[1][1][0][0] = 5;
        b.field12[1][1][0][1] = 6;
        b.field12[1][1][1][0] = 7;
        b.field12[1][1][1][1] = 0;
        b.field12[2][0][0][0] = 1;
        b.field12[2][0][0][1] = 2;
        b.field12[2][0][1][0] = 3;
        b.field12[2][0][1][1] = 4;
        b.field12[2][1][0][0] = 5;
        b.field12[2][1][0][1] = 6;
        b.field12[2][1][1][0] = 7;
        b.field12[2][1][1][1] = 0;
        b.field15 = 0b1000100;
        b.field16 = 0b0000010;
        b.field19 = 0b1000100;
        b.field20 = 0b0000010;

        TS_ASSERT_EQUALS(__bitfield_t_encoded_array_size(&b, 1), expectEncSize);

        uint8_t buf[expectEncSize];
        TS_ASSERT_EQUALS(__bitfield_t_encode_array(&buf, 0, expectEncSize, &b, 1), expectEncSize);

        for (size_t i = 0; i < expectEncSize; ++i) {
            string msg = "Byte index " + to_string(i) + " didn't match expected encoded value";
            TSM_ASSERT_EQUALS(msg.c_str(), buf[i], enc[i]);
        }
    }

    void testCDecode()
    {
        bitfield_t b;
        TS_ASSERT_EQUALS(__bitfield_t_decode_array(enc, 0, expectEncSize, &b, 1), expectEncSize);

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
        TS_ASSERT_EQUALS(b.field9, -(1 << 27));
        TS_ASSERT_EQUALS(b.field10, ((uint64_t)1 << 52) | 1);
        TS_ASSERT_EQUALS(b.field11, 3);
        TS_ASSERT_EQUALS(b.field12[0][0][0][0], 1);
        TS_ASSERT_EQUALS(b.field12[0][0][0][1], 2);
        TS_ASSERT_EQUALS(b.field12[0][0][1][0], 3);
        TS_ASSERT_EQUALS(b.field12[0][0][1][1], 4);
        TS_ASSERT_EQUALS(b.field12[0][1][0][0], 5);
        TS_ASSERT_EQUALS(b.field12[0][1][0][1], 6);
        TS_ASSERT_EQUALS(b.field12[0][1][1][0], 7);
        TS_ASSERT_EQUALS(b.field12[0][1][1][1], 0);
        TS_ASSERT_EQUALS(b.field12[1][0][0][0], 1);
        TS_ASSERT_EQUALS(b.field12[1][0][0][1], 2);
        TS_ASSERT_EQUALS(b.field12[1][0][1][0], 3);
        TS_ASSERT_EQUALS(b.field12[1][0][1][1], 4);
        TS_ASSERT_EQUALS(b.field12[1][1][0][0], 5);
        TS_ASSERT_EQUALS(b.field12[1][1][0][1], 6);
        TS_ASSERT_EQUALS(b.field12[1][1][1][0], 7);
        TS_ASSERT_EQUALS(b.field12[1][1][1][1], 0);
        TS_ASSERT_EQUALS(b.field12[2][0][0][0], 1);
        TS_ASSERT_EQUALS(b.field12[2][0][0][1], 2);
        TS_ASSERT_EQUALS(b.field12[2][0][1][0], 3);
        TS_ASSERT_EQUALS(b.field12[2][0][1][1], 4);
        TS_ASSERT_EQUALS(b.field12[2][1][0][0], 5);
        TS_ASSERT_EQUALS(b.field12[2][1][0][1], 6);
        TS_ASSERT_EQUALS(b.field12[2][1][1][0], 7);
        TS_ASSERT_EQUALS(b.field12[2][1][1][1], 0);
        TS_ASSERT_EQUALS(b.field15, -60);
        TS_ASSERT_EQUALS(b.field16, 2);
        TS_ASSERT_EQUALS(b.field19, 68);
        TS_ASSERT_EQUALS(b.field20, 2);
    }

    void testCppEncode()
    {
        cpp::bitfield_t b = {};
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
        b.field11 = 3;
        b.field12[0][0][0][0] = 1;
        b.field12[0][0][0][1] = 2;
        b.field12[0][0][1][0] = 3;
        b.field12[0][0][1][1] = 4;
        b.field12[0][1][0][0] = 5;
        b.field12[0][1][0][1] = 6;
        b.field12[0][1][1][0] = 7;
        b.field12[0][1][1][1] = 0;
        b.field12[1][0][0][0] = 1;
        b.field12[1][0][0][1] = 2;
        b.field12[1][0][1][0] = 3;
        b.field12[1][0][1][1] = 4;
        b.field12[1][1][0][0] = 5;
        b.field12[1][1][0][1] = 6;
        b.field12[1][1][1][0] = 7;
        b.field12[1][1][1][1] = 0;
        b.field12[2][0][0][0] = 1;
        b.field12[2][0][0][1] = 2;
        b.field12[2][0][1][0] = 3;
        b.field12[2][0][1][1] = 4;
        b.field12[2][1][0][0] = 5;
        b.field12[2][1][0][1] = 6;
        b.field12[2][1][1][0] = 7;
        b.field12[2][1][1][1] = 0;
        b.field15 = 0b1000100;
        b.field16 = 0b0000010;
        b.field19 = 0b1000100;
        b.field20 = 0b0000010;

        TS_ASSERT_EQUALS(b._getEncodedSizeNoHash(), expectEncSize);

        uint8_t buf[expectEncSize];
        TS_ASSERT_EQUALS(b._encodeNoHash(&buf, 0, expectEncSize), expectEncSize);

        for (size_t i = 0; i < expectEncSize; ++i) {
            string msg = "Byte index " + to_string(i) + " didn't match expected encoded value";
            TSM_ASSERT_EQUALS(msg.c_str(), buf[i], enc[i]);
        }
    }

    void testCppDecode()
    {
        cpp::bitfield_t b;
        TS_ASSERT_EQUALS(b._decodeNoHash(enc, 0, expectEncSize), expectEncSize);

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
        TS_ASSERT_EQUALS(b.field9, -(1 << 27));
        TS_ASSERT_EQUALS(b.field10, ((uint64_t)1 << 52) | 1);
        TS_ASSERT_EQUALS(b.field11, 3);
        TS_ASSERT_EQUALS(b.field12[0][0][0][0], 1);
        TS_ASSERT_EQUALS(b.field12[0][0][0][1], 2);
        TS_ASSERT_EQUALS(b.field12[0][0][1][0], 3);
        TS_ASSERT_EQUALS(b.field12[0][0][1][1], 4);
        TS_ASSERT_EQUALS(b.field12[0][1][0][0], 5);
        TS_ASSERT_EQUALS(b.field12[0][1][0][1], 6);
        TS_ASSERT_EQUALS(b.field12[0][1][1][0], 7);
        TS_ASSERT_EQUALS(b.field12[0][1][1][1], 0);
        TS_ASSERT_EQUALS(b.field12[1][0][0][0], 1);
        TS_ASSERT_EQUALS(b.field12[1][0][0][1], 2);
        TS_ASSERT_EQUALS(b.field12[1][0][1][0], 3);
        TS_ASSERT_EQUALS(b.field12[1][0][1][1], 4);
        TS_ASSERT_EQUALS(b.field12[1][1][0][0], 5);
        TS_ASSERT_EQUALS(b.field12[1][1][0][1], 6);
        TS_ASSERT_EQUALS(b.field12[1][1][1][0], 7);
        TS_ASSERT_EQUALS(b.field12[1][1][1][1], 0);
        TS_ASSERT_EQUALS(b.field12[2][0][0][0], 1);
        TS_ASSERT_EQUALS(b.field12[2][0][0][1], 2);
        TS_ASSERT_EQUALS(b.field12[2][0][1][0], 3);
        TS_ASSERT_EQUALS(b.field12[2][0][1][1], 4);
        TS_ASSERT_EQUALS(b.field12[2][1][0][0], 5);
        TS_ASSERT_EQUALS(b.field12[2][1][0][1], 6);
        TS_ASSERT_EQUALS(b.field12[2][1][1][0], 7);
        TS_ASSERT_EQUALS(b.field12[2][1][1][1], 0);
        TS_ASSERT_EQUALS(b.field15, -60);
        TS_ASSERT_EQUALS(b.field16, 2);
        TS_ASSERT_EQUALS(b.field19, 68);
        TS_ASSERT_EQUALS(b.field20, 2);
    }
};
