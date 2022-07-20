#!/usr/bin/python

from zerocm import ZCM
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
from example_t import example_t
import time

success1 = False
def handler(channel, msg):
    global success1
    assert msg.timestamp == 10
    success1 = True

success2 = False
def handler_raw(channel, data):
    global success2
    success2 = True

success3 = False
def handler_with_recv_timestamp(channel, msg, rt):
    global success3
    assert rt != 0
    success3 = True

# make a new zcm object and launch the handle thread
zcm = ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

# declare a new msg and populate it
msg = example_t()
msg.timestamp = 10

# set up a subscription on channel "TEST"
subs1 = zcm.subscribe    ("TEST", example_t, handler)
subs2 = zcm.subscribe_raw("TEST",            handler_raw)
subs3 = zcm.subscribe    ("TEST", example_t, handler_with_recv_timestamp)

# publish a message
zcm.publish("TEST", msg)

# wait a second
time.sleep(1)

# publish another
zcm.publish("TEST", msg)

# handle incoming message
zcm.handle()

# clean up
zcm.unsubscribe(subs1)
zcm.unsubscribe(subs2)
zcm.unsubscribe(subs3)

# notify user of success
print("Success" if success1 and success2 and success3 else "Failure")
