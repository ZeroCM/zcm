import zmq

PUB_ADDR = 'ipc:///tmp/pub';
SUB_ADDR = 'ipc:///tmp/sub';

class zcomm:

    def __init__(self):
        self.ctx = zmq.Context()
        self.pub = self.ctx.socket(zmq.PUB)
        self.pub.connect(PUB_ADDR)
        self.sub = self.ctx.socket(zmq.SUB)
        self.sub.connect(SUB_ADDR)
        self.callbacks = {} # maps channels -> callbacks

    def subscribe(self, channel, callback):
        self.sub.setsockopt(zmq.SUBSCRIBE, channel)
        self.callbacks[channel] = callback

    def publish(self, channel, data):
        self.pub.send_multipart([channel, data])

    def handle(self):
        channel, msg = self.sub.recv_multipart()
        if channel in self.callbacks:
            self.callbacks[channel](channel, msg)
        elif '' in self.callbacks:
            self.callbacks[''](channel, msg)

    def run(self):
        while True:
            self.handle()
