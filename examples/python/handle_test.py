#!/usr/bin/python

import zerocm
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
from example_t import example_t
import time

success = "Failure"
def handler(channel, msg):
    global success
    assert msg.timestamp == 10
    success = "Success"

# make a new zcm object and launch the handle thread
zcm = zerocm.ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

# declare a new msg and populate it
msg = example_t()
msg.timestamp = 10

ret = zcm.handle(0)
if ret != zerocm.ZCM_EAGAIN:
    print("Failed to return successfully when no message is ready")
    sys.exit(1)

# set up a subscription on channel "TEST"
subs = zcm.subscribe("TEST", example_t, handler)

# publish a message
zcm.publish("TEST", msg)

# wait for ipc to start up
time.sleep(1)

# publish another
zcm.publish("TEST", msg)

# wait for receive thread to enqueue message
time.sleep(0.5)

# handle incoming message
ret = zcm.handle(0)
if ret != zerocm.ZCM_EOK:
    print("Failed to return successfully when a message is ready")
    sys.exit(1)

# clean up
zcm.unsubscribe(subs)

# notify user of success
print(success)
