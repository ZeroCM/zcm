#!/usr/bin/python

from zcm import ZCM
import sys
sys.path.insert(0, '../build/types/')
from example_t import example_t
import time
import threading

import signal
def signal_handler(signal, frame):
    sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

lock = threading.Lock()
done = 0
def handler(channel, msg):
    global done
    print "Received message on channel: " + channel
    assert msg.timestamp == 10
    lock.acquire()
    done = done + 1
    lock.release()

zcm = ZCM()
if not zcm.good():
    print "Unable to initialize zcm"
    exit()
zcm.start()
msg = example_t()
msg.timestamp = 10
subs = zcm.subscribe("TEST", example_t, handler)
while True:
    zcm.publish("TEST", msg)
    lock.acquire()
    _done = done
    lock.release()
    if _done == 10:
        break
    time.sleep(0.25)

zcm.unsubscribe(subs)
zcm.stop()

print "Success"
