#!/usr/bin/python

import zerocm
import sys
sys.path.insert(0, '../build/types/')
from example_t import example_t
import time

import signal
def signal_handler(signal, frame):
    sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

done = 0
def handler(channel, msg):
    global done
    print("Received message on channel: " + channel)
    assert msg.timestamp == 10
    done = done + 1

z = zcm.ZCM()
if not z.good():
    print("Unable to initialize zcm")
    exit()
z.start()

msg = example_t()
msg.timestamp = 10
subs = z.subscribe("TEST", example_t, handler)

assert z.publish("TEST", msg) == zcm.ZCM_EOK
time.sleep(1)

z.setQueueSize(10)
z.pause()
for i in range(0,5):
    assert z.publish("TEST", msg) == zcm.ZCM_EOK

start = time.time()
while done != 5:
    z.flush()
    time.sleep(0) # yield the gil
    if (time.time() - start > 2):
        print("Failure")
        sys.exit(1)

z.resume()
while True:
    z.publish("TEST", msg)
    if done == 10:
        break
    time.sleep(0.25)

z.unsubscribe(subs)
z.stop()

print("Success")
