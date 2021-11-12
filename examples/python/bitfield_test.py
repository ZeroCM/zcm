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
    assert msg.field19 == 68
    assert msg.field20 == 2
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
b.field19 = 0b1000100;
b.field20 = 0b0000010;

zcm.start()
for i in range(0,5):
    zcm.publish("BITFIELD", b)
    time.sleep(0.2)
zcm.stop()

print(success)
