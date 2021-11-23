#!/usr/bin/python

from zerocm import ZCM
import sys, os
blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/tests/test/'
sys.path.insert(0, blddir + "types/")
from example_t import example_t
import time

success = "Failure"
def handler(channel, msg):
    global success

    assert(msg.utime == 10)
    assert(msg.position[0] == 1)
    assert(msg.position[1] == 2)
    assert(msg.position[2] == 3.5)
    assert(msg.orientation[0] == 1.1)
    assert(msg.orientation[1] == 2.2)
    assert(msg.orientation[2] == 3.3)
    assert(msg.orientation[3] == 4.4)
    assert(msg.num_ranges == 2)
    assert(msg.num_ranges == len(msg.ranges))
    assert(msg.ranges[0] == 10)
    assert(msg.ranges[1] == 20)
    assert(msg.name == "this is a test")
    assert(msg.enabled == True)

    success = "Success"

# make a new zcm object and launch the handle thread
zcm = ZCM("inproc")
if not zcm.good():
    print("Unable to initialize zcm")
    exit()

subs = zcm.subscribe("EXAMPLE", example_t, handler)

msg = example_t()
msg.utime = 10;
msg.position = [1, 2, 3.5];
msg.orientation = [1.1, 2.2, 3.3, 4.4];
msg.ranges = [10, 20]
msg.num_ranges = len(msg.ranges)
msg.name = "this is a test"
msg.ignore0 = 0
msg.ignore1 = 0
msg.ignore2 = 0
msg.enabled = True

assert(msg.test_const_8_max_hex  == -1);
assert(msg.test_const_16_max_hex == -1);
assert(msg.test_const_32_max_hex == -1);
assert(msg.test_const_64_max_hex == -1);

assert(msg.test_const_float  == 1e-20);
assert(msg.test_const_double == 12.1e200);

zcm.start()
for i in range(0,5):
    zcm.publish("EXAMPLE", msg)
    time.sleep(0.2)
zcm.stop()

sys.exit(0 if success == "Success" else 1)
