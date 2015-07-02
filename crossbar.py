#!/usr/bin/env python
import zmq

SUB_ADDR = 'ipc:///tmp/sub'
PUB_ADDR = 'ipc:///tmp/pub'

def main():

    try:
        context = zmq.Context(1)

        userpub = context.socket(zmq.SUB)
        userpub.bind(PUB_ADDR)
        userpub.setsockopt(zmq.SUBSCRIBE, "")

        usersub = context.socket(zmq.PUB)
        usersub.bind(SUB_ADDR)

        zmq.device(zmq.FORWARDER, userpub, usersub)
    except Exception, e:
        print e
        print "bringing down zmq device"
    except KeyboardInterrupt:
        pass
    finally:
        pass
        userpub.close()
        usersub.close()
        context.term()

if __name__ == "__main__":
    main()
