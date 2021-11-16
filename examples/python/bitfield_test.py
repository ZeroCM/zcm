#!/usr/bin/python

from zerocm import ZCM
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
from bitfield_t import bitfield_t
import time

success = "Failure"
def handler(channel, msg):
    global success
    assert msg.field1 == -1
    assert msg.field2[0][0] == -1
    assert msg.field2[0][1] ==  0
    assert msg.field2[0][2] == -1
    assert msg.field2[0][3] ==  0
    assert msg.field2[1][0] == -1
    assert msg.field2[1][1] ==  0
    assert msg.field2[1][2] == -1
    assert msg.field2[1][3] ==  0
    assert msg.field3 == -1
    assert msg.field4 == -3
    assert msg.field5 == 7
    assert msg.field6 == 0
    assert msg.field7 == 0
    assert msg.field8_dim1 == 0
    assert msg.field8_dim2 == 0
    assert msg.field8 == []
    assert msg.field9 == ~(1 << 27) + 1
    assert msg.field10 == (1 << 52) | 1
    assert msg.field11 == 3
    for i in range(0, 3):
        for j in range(0, 2):
            for k in range(0, 2):
                for l in range(0, 2):
                    assert msg.field12[i][j][k][l] == k + l + 1
    assert msg.field15 == -60
    assert msg.field16 == 2
    assert msg.field18 == 15
    assert msg.field19 == 68
    assert msg.field20 == 2
    assert msg.field22 == msg.FIELD22_TEST
    assert msg.field23 == msg.FIELD23_TEST
    assert msg.field24 == msg.FIELD24_TEST
    assert msg.field25 == 3
    assert msg.field26 == 255
    assert msg.field27 == 3
    assert msg.field28 == 0x7f
    assert msg.field29 == 3
    assert msg.field30 == 0x7fff
    assert msg.field31 == 0xf
    assert msg.field32 == 0x7fffffff
    assert msg.field33 == 0xf
    assert msg.field34 == 0x7fffffffffffffff
    success = "Success"

# make a new zcm object and launch the handle thread
zcm = ZCM("inproc")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

# set up a subscription on channel "TEST"
subs = zcm.subscribe("BITFIELD", bitfield_t, handler)

b = bitfield_t()
b.field1 = 3;
b.field2 = [ [1, 0, 1, 0], [1, 0, 1, 0] ];
b.field3 = 0xf;
b.field4 = 5;
b.field5 = 7;
b.field9 = 1 << 27;
b.field10 = (1 << 52) | 1;
b.field11 = 3
b.field12 = [[],[],[]]
for i in range(0, 3):
    b.field12[i] = [[], []]
    for j in range(0, 2):
        b.field12[i][j] = [[], []]
        for k in range(0, 2):
            for l in range(0, 2):
                b.field12[i][j][k].append(k + l + 1)
b.field15 = 0b1000100;
b.field16 = 0b0000010;
b.field18 = -1;
b.field19 = 0b1000100;
b.field20 = 0b0000010;
b.field22 = b.FIELD22_TEST;
b.field23 = b.FIELD23_TEST;
b.field24 = b.FIELD24_TEST;
b.field25 = 0xff;
b.field26 = 0xff;
b.field27 = 0x7f;
b.field28 = 0x7f;
b.field29 = 0x7fff;
b.field30 = 0x7fff;
b.field31 = 0x7fffffff;
b.field32 = 0x7fffffff;
b.field33 = 0x7fffffffffffffff;
b.field34 = 0x7fffffffffffffff;

assert(b.FIELD22_TEST == 255);
assert(b.FIELD23_TEST ==   3);
assert(b.FIELD24_TEST ==   7);

assert(b.SIGN_TEST_0  == 0x0f);
assert(b.SIGN_TEST_1  ==  -16);
assert(b.SIGN_TEST_2  == 0x7f);
assert(b.SIGN_TEST_3  == -128);

assert(b.SIGN_TEST_4  == 0x1fff);
assert(b.SIGN_TEST_5  ==  -8192);
assert(b.SIGN_TEST_6  == 0x7fff);
assert(b.SIGN_TEST_7  == -32768);

assert(b.SIGN_TEST_8  ==  0x01ffffff);
assert(b.SIGN_TEST_9  ==   -33554432);
assert(b.SIGN_TEST_10 ==  0x7fffffff);
assert(b.SIGN_TEST_11 == -2147483648);

assert(b.SIGN_TEST_12 == -1);
assert(b.SIGN_TEST_13 == 72057594037927935);
assert(b.SIGN_TEST_14 == -72057594037927936);
assert(b.SIGN_TEST_15 == 9223372036854775807);
assert(b.SIGN_TEST_16 == -9223372036854775808);

assert(b.SIGN_TEST_17 == 7);
assert(b.SIGN_TEST_18 == 0x7f);
assert(b.SIGN_TEST_19 == 7);
assert(b.SIGN_TEST_20 == 0x7f);
assert(b.SIGN_TEST_21 == 7);
assert(b.SIGN_TEST_22 == 0x7fff);
assert(b.SIGN_TEST_23 == 7);
assert(b.SIGN_TEST_24 == 0x7fffffff);
assert(b.SIGN_TEST_25 == 1);
assert(b.SIGN_TEST_26 == 7);
assert(b.SIGN_TEST_27 == 9223372036854775807);

assert(b.SIGN_TEST_28 == 0x7f);
assert(b.SIGN_TEST_29 == 0xff);
assert(b.SIGN_TEST_30 == 0x7f);
assert(b.SIGN_TEST_31 == -1);
assert(b.SIGN_TEST_32 == 127);
assert(b.SIGN_TEST_33 == -1);
assert(b.SIGN_TEST_34 == 0x7fff);
assert(b.SIGN_TEST_35 == -1);
assert(b.SIGN_TEST_36 == 32767);
assert(b.SIGN_TEST_37 == -1);
assert(b.SIGN_TEST_38 == 0x7fffffff);
assert(b.SIGN_TEST_39 == -1);
assert(b.SIGN_TEST_40 == 2147483647);
assert(b.SIGN_TEST_41 == -1);
assert(b.SIGN_TEST_42 == 0x7fffffffffffffff);
assert(b.SIGN_TEST_43 == -1);
assert(b.SIGN_TEST_44 == 9223372036854775807);
assert(b.SIGN_TEST_45 == -1);

zcm.start()
for i in range(0,5):
    zcm.publish("BITFIELD", b)
    time.sleep(0.2)
zcm.stop()

print(success)
