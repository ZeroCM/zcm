import sys
sys.path.append('../../zcm-python')

import zcm
import time

from example_t import example_t

z = zcm.ZCM()

msg = example_t()
msg.timestamp = int(time.time() * 1000000)
msg.position = (1, 2, 3)
msg.orientation = (1, 0, 0, 0)
msg.ranges = range(15)
msg.num_ranges = len(msg.ranges)
msg.name = "example string"
msg.enabled = True

while True:
    z.publish("EXAMPLE", msg.encode())
    time.sleep(1)
