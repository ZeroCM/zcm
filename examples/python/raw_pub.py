#!/usr/bin/python

from zerocm import ZCM
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
import time

success = "Failure"
def handler(channel, data):
    global success
    if data.decode('utf-8') == "test":
        success = "Success"

# make a new zcm object and launch the handle thread
zcm = ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

msg = "test".encode('utf-8')

# set up a subscription on channel "TEST"
subs = zcm.subscribe_raw("TEST", handler)

# publish a message
zcm.publish_raw("TEST", msg)

# wait a second
time.sleep(1)

# publish another
zcm.publish_raw("TEST", msg)

# handle incoming message
zcm.handle()

# clean up
zcm.unsubscribe(subs)

# notify user of success
print(success)
