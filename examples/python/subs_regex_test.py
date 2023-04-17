#!/usr/bin/python

from zerocm import ZCM
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
from example_t import example_t
import time

success1 = False
success2 = False
successZ = False
def handler(channel, msg):
    print(channel)
    global success1
    global success2
    global successZ
    if channel == "TEST_1":
        success1 = True
    if channel == "TEST_2":
        success2 = True
    if channel == "TEST_Z":
        successZ = True

def handler_ignore(channel, msg):
    print(channel)

# make a new zcm object and launch the handle thread
zcm = ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

# declare a new msg and populate it
msg = example_t()
msg.timestamp = 10

# set up a subscription on channel "TEST"
print("Subscribing")
subs1 = zcm.subscribe("TEST.*", example_t, handler)
subs2 = zcm.subscribe("TEST.*", example_t, handler_ignore)
subs3 = zcm.subscribe("TEST", example_t, handler_ignore)

zcm.start()

# publish a message
print("Publishing")
zcm.publish("TEST_1", msg)
zcm.publish("TEST_2", msg)
zcm.publish("TEST_Z", msg)

# wait a second
print("Sleeping")
time.sleep(1)

print("Publishing again")
# publish another
zcm.publish("TEST_1", msg)
zcm.publish("TEST_2", msg)
zcm.publish("TEST_Z", msg)

# handle incoming message
print("Handling")
zcm.flush()

# wait a second
print("Sleeping")
time.sleep(1)

# handle incoming message
print("Stopping")
zcm.stop()

# clean up
print("Unsubscribing")
zcm.unsubscribe(subs1)
zcm.unsubscribe(subs2)
zcm.unsubscribe(subs3)

# notify user of success
print("Success" if success1 and success2 and successZ else "Failure")
