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

    static constexpr size_t expectEncSize = 57;
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
        0b10000000, // field20
        0,          // field21
        0b11111111, // field22
        0b01111100, // fields 23 and 24
        0,          // filler1
        0b11111111, // fields 25 and 26
        0b11111111, // fields 26, 27, and 28
        0b11111111, // fields 28, 29, and 30
        0b11111111, // field30
        0b11111111, // fields 30 and 31
        0b11111111, // field32
        0b11111111, // field32
        0b11111111, // field32
        0b11111111, // fields 32 and 33
        0b11111111, // fields 33 and 34
        0b11111111, // field34
        0b11111111, // field34
        0b11111111, // field34
        0b11111111, // field34
        0b11111111, // field34
        0b11111111, // field34
        0b11111111, // field34
        0b11000000, // field34
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
        b.field22 = BITFIELD_T_FIELD22_TEST;
        b.field23 = BITFIELD_T_FIELD23_TEST;
        b.field24 = BITFIELD_T_FIELD24_TEST;
        b.field25 = 0xff;
        b.field26 = 0xff;
        b.field27 = 0xff;
        b.field28 = 0xff;
        b.field29 = 0xffff;
        b.field30 = 0xffff;
        b.field31 = 0xffffffff;
        b.field32 = 0xffffffff;
        b.field33 = 0xffffffffffffffff;
        b.field34 = 0xffffffffffffffff;

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
        TS_ASSERT_EQUALS(b.field22, BITFIELD_T_FIELD22_TEST);
        TS_ASSERT_EQUALS(b.field23, BITFIELD_T_FIELD23_TEST);
        TS_ASSERT_EQUALS(b.field24, BITFIELD_T_FIELD24_TEST);
        TS_ASSERT_EQUALS(b.field25, 3);
        TS_ASSERT_EQUALS(b.field26, 255);
        TS_ASSERT_EQUALS(b.field27, 3);
        TS_ASSERT_EQUALS(b.field28, 0x7f);
        TS_ASSERT_EQUALS(b.field29, 3);
        TS_ASSERT_EQUALS(b.field30, 0x7fff);
        TS_ASSERT_EQUALS(b.field31, 0xf);
        TS_ASSERT_EQUALS(b.field32, 0x7fffffff);
        TS_ASSERT_EQUALS(b.field33, 0xf);
        TS_ASSERT_EQUALS(b.field34, 0x7fffffffffffffff);
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
        b.field22 = b.FIELD22_TEST;
        b.field23 = b.FIELD23_TEST;
        b.field24 = b.FIELD24_TEST;
        b.field25 = 0xff;
        b.field26 = 0xff;
        b.field27 = 0xff;
        b.field28 = 0xff;
        b.field29 = 0xffff;
        b.field30 = 0xffff;
        b.field31 = 0xffffffff;
        b.field32 = 0xffffffff;
        b.field33 = 0xffffffffffffffffL;
        b.field34 = 0xffffffffffffffffL;

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
        TS_ASSERT_EQUALS(b.field22, b.FIELD22_TEST);
        TS_ASSERT_EQUALS(b.field23, b.FIELD23_TEST);
        TS_ASSERT_EQUALS(b.field24, b.FIELD24_TEST);
        TS_ASSERT_EQUALS(b.field25, 3);
        TS_ASSERT_EQUALS(b.field26, 255);
        TS_ASSERT_EQUALS(b.field27, 3);
        TS_ASSERT_EQUALS(b.field28, 0x7f);
        TS_ASSERT_EQUALS(b.field29, 3);
        TS_ASSERT_EQUALS(b.field30, 0x7fff);
        TS_ASSERT_EQUALS(b.field31, 0xf);
        TS_ASSERT_EQUALS(b.field32, 0x7fffffff);
        TS_ASSERT_EQUALS(b.field33, 0xf);
        TS_ASSERT_EQUALS(b.field34, 0x7fffffffffffffff);
    }

    void testCConsts()
    {
        TS_ASSERT_EQUALS(BITFIELD_T_FIELD22_TEST , 255);
        TS_ASSERT_EQUALS(BITFIELD_T_FIELD23_TEST ,   3);
        TS_ASSERT_EQUALS(BITFIELD_T_FIELD24_TEST ,   7);

        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_0 , (int8_t)0x0f);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_1 , (int8_t)0xf0);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_2 , (int8_t)0x7f);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_3 , (int8_t)0x80);

        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_4 , (int16_t)0x1fff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_5 , (int16_t)0xe000);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_6 , (int16_t)0x7fff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_7 , (int16_t)0x8000);

        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_8 , (int32_t)0x01ffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_9 , (int32_t)0xfe000000);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_10, (int32_t)0x7fffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_11, (int32_t)0x80000000);

        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_12, (int64_t)0xffffffffffffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_13, (int64_t)0x00ffffffffffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_14, (int64_t)0xff00000000000000);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_15, (int64_t)0x7fffffffffffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_16, (int64_t)0x8000000000000000);

        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_17, 0x07);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_18, 0x7f);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_19, 0x07);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_20, 0x7f);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_21, 0x0007);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_22, 0x7fff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_23, 0x00000007);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_24, 0x7fffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_25, 0x0000000000000001);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_26, 0x0000000000000007);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_27, 0x7fffffffffffffff);

        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_28, 0x7f);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_29, 0xff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_30, 0x7f);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_31, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_32, 127);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_33, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_34, 0x7fff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_35, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_36, 32767);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_37, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_38, 0x7fffffff);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_39, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_40, 2147483647);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_41, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_42, 0x7fffffffffffffffL);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_43, -1);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_44, 9223372036854775807L);
        TS_ASSERT_EQUALS(BITFIELD_T_SIGN_TEST_45, -1);
    }

    void testCppConsts()
    {
        TS_ASSERT_EQUALS(cpp::bitfield_t::FIELD22_TEST , 255);
        TS_ASSERT_EQUALS(cpp::bitfield_t::FIELD23_TEST ,   3);
        TS_ASSERT_EQUALS(cpp::bitfield_t::FIELD24_TEST ,   7);

        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_0 , (int8_t)0x0f);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_1 , (int8_t)0xf0);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_2 , (int8_t)0x7f);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_3 , (int8_t)0x80);

        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_4 , (int16_t)0x1fff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_5 , (int16_t)0xe000);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_6 , (int16_t)0x7fff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_7 , (int16_t)0x8000);

        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_8 , (int32_t)0x01ffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_9 , (int32_t)0xfe000000);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_10, (int32_t)0x7fffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_11, (int32_t)0x80000000);

        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_12, (int64_t)0xffffffffffffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_13, (int64_t)0x00ffffffffffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_14, (int64_t)0xff00000000000000);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_15, (int64_t)0x7fffffffffffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_16, (int64_t)0x8000000000000000);

        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_17, 0x07);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_18, 0x7f);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_19, 0x07);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_20, 0x7f);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_21, 0x0007);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_22, 0x7fff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_23, 0x00000007);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_24, 0x7fffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_25, 0x0000000000000001);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_26, 0x0000000000000007);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_27, 0x7fffffffffffffff);

        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_28, 0x7f);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_29, 0xff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_30, 0x7f);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_31, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_32, 127);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_33, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_34, 0x7fff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_35, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_36, 32767);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_37, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_38, 0x7fffffff);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_39, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_40, 2147483647);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_41, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_42, 0x7fffffffffffffffL);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_43, -1);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_44, 9223372036854775807L);
        TS_ASSERT_EQUALS(cpp::bitfield_t::SIGN_TEST_45, -1);
    }
};
