#!/usr/bin/python

from zerocm import ZCM
import sys
sys.path.insert(0, '../build/types/')
from example_t import example_t
from encoded_t import encoded_t
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

encoded = encoded_t()
encoded.msg = msg.encode()
encoded.n   = len(encoded.msg)

decoded = example_t.decode(encoded.msg)
assert(decoded.timestamp == msg.timestamp)

print("Success")
