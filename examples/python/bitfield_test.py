#!/usr/bin/python

from zerocm import ZCM
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
from bitfield_t import bitfield_t
import time

def handler(channel, msg):
    print(msg.field1)
    print(msg.field2)
    print(msg.field3)
    print(msg.field4)
    print(msg.field5)
    print(msg.field6)
    print(msg.field7)
    print(msg.field8_dim1)
    print(msg.field8_dim2)
    print(msg.field8)
    print(msg.field9)
    print(msg.field10)
    print(msg.field11)

# make a new zcm object and launch the handle thread
zcm = ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

# set up a subscription on channel "TEST"
subs = zcm.subscribe("BITFIELD", bitfield_t, handler)

zcm.start()
while True:
    time.sleep(1)
zcm.stop()
