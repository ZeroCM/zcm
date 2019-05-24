#!/usr/bin/python

from zerocm import ZCM
import sys
sys.path.insert(0, '../build/types/')
from example_t import example_t
import time

success = "Failure"
def handler(channel, msg):
    global success
    assert msg.timestamp == 10
    success = "Success"

# make a new zcm object and launch the handle thread
zcm = ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

# declare a new msg and populate it
msg = example_t()
msg.timestamp = 10

# set up a subscription on channel "TEST"
subs = zcm.subscribe("TEST", example_t, handler)

# publish a message
zcm.publish("TEST", msg)

# wait a second
time.sleep(1)

# publish another
zcm.publish("TEST", msg)

# handle incoming message
zcm.handle()

# clean up
zcm.unsubscribe(subs)

# notify user of success
print(success)
