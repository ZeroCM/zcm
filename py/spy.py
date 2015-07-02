#!/usr/bin/env python
import sys
import time
from zcomm import zcomm

def handle_msg(channel, data):
    print '   channel:%s, data:%s' % (channel, data)

def main(argv):
    z = zcomm()
    z.subscribe('', handle_msg)
    z.run()

if __name__ == "__main__":
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
