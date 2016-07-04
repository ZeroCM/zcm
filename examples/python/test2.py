from zcm import ZCM
import sys
sys.path.insert(0, '../build/types/')
from example_t import example_t
import time

def handler(channel, msg):
    print channel

# make a new zcm object and launch the handle thread
zcm = ZCM("")

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