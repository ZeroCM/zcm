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
    failed = False
    failed = failed or msg.field1 != -1
    failed = failed or msg.field2[0][0] != -1
    failed = failed or msg.field2[0][1] !=  0
    failed = failed or msg.field2[0][2] != -1
    failed = failed or msg.field2[0][3] !=  0
    failed = failed or msg.field2[1][0] != -1
    failed = failed or msg.field2[1][1] !=  0
    failed = failed or msg.field2[1][2] != -1
    failed = failed or msg.field2[1][3] !=  0
    failed = failed or msg.field3 != -1
    failed = failed or msg.field4 != -3
    failed = failed or msg.field5 != 7
    failed = failed or msg.field6 != 0
    failed = failed or msg.field7 != 0
    failed = failed or msg.field8_dim1 != 0
    failed = failed or msg.field8_dim2 != 0
    failed = failed or msg.field8 != []
    failed = failed or msg.field9 != ~(1 << 27) + 1
    failed = failed or msg.field10 != (1 << 52) | 1
    success = "Failed to decode properly" if failed else "Success"

# make a new zcm object and launch the handle thread
zcm = ZCM("")
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
b.field12 = [ [ [ [ 1, 2 ], [ 1, 2 ] ], [ [ 1, 2 ], [ 1, 2 ] ] ], [ [ [ 1, 2 ], [ 1, 2 ] ], [ [ 1, 2 ], [ 1, 2 ] ] ], [ [ [ 1, 2 ], [ 1, 2 ] ], [ [ 1, 2 ], [ 1, 2 ] ] ] ]

zcm.start()
for i in range(0,5):
    zcm.publish("BITFIELD", b)
    time.sleep(0.2)
zcm.stop()

print(success)
