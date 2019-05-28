#!/usr/bin/python

from zerocm import ZCM
import sys
sys.path.insert(0, '../build/types/')
import time

success = "Failure"
def handler(channel, data):
    global success
    if data == "test":
        success = "Success"

# make a new zcm object and launch the handle thread
zcm = ZCM("")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

msg = "test"

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
