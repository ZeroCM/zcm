#!/usr/bin/env python
import sys
import time
import zmq

SUB_ADDR = 'ipc:///tmp/sub'
PUB_ADDR = 'ipc:///tmp/pub'

def main(argv):
    ctx = zmq.Context()
    s = ctx.socket(zmq.SUB)
    s.connect(SUB_ADDR)
    s.setsockopt(zmq.SUBSCRIBE,'')

    while True:
        channel, msg = s.recv_multipart()
        print '   channel:%s, msg:%s' % (channel, msg)

if __name__ == "__main__":
    try:
        main(sys.argv)
    except KeyboardInterrupt:
        pass
