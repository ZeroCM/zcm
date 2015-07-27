import sys
sys.path.append('../../zcm/python')

import zcm
import time

from example_t import example_t

def my_handler(channel, data):
    msg = example_t.decode(data)
    print("Received message on channel \"%s\"" % channel)
    print("   timestamp   = %s" % str(msg.timestamp))
    print("   position    = %s" % str(msg.position))
    print("   orientation = %s" % str(msg.orientation))
    print("   ranges: %s" % str(msg.ranges))
    print("   name        = '%s'" % msg.name)
    print("   enabled     = %s" % str(msg.enabled))
    print("")

z = zcm.ZCM()
z.subscribe("EXAMPLE", my_handler)

try:
    while True:
        z.handle()
except KeyboardInterrupt:
    pass
