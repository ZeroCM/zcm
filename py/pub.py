#!/usr/bin/env python
import itertools
import sys
import time
from zcomm import zcomm

HZ = 1

def main(argv):
    z = zcomm()

    msg_counter = itertools.count()
    while True:
        msg = str(msg_counter.next())
        z.publish('FROB_DATA', msg);
        time.sleep(1/float(HZ))

if __name__ == "__main__":
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
