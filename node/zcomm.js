var zmq = require('zmq');

var PUB_ADDR = 'ipc:///tmp/pub';
var SUB_ADDR = 'ipc:///tmp/sub';

function create() {
    var sub = zmq.socket('sub');
    sub.connect(SUB_ADDR);

    var pub = zmq.socket('pub');
    pub.connect(PUB_ADDR);

    // Channel -> Callback
    var callbacks = {};
    var callback_all = null;

    sub.on('message', function(channel, data) {
        if (channel in callbacks)
            callbacks[channel](channel, data);
        if (callback_all)
            callback_all(channel, data)
    });

    return {
        subscribe: function(channel, cb) {
            sub.subscribe(channel);
            callbacks[channel] = cb;
            if (channel == '')
                callback_all = cb
        },
        publish: function(channel, data) {
            pub.send([channel, data]);
        }
    };
}

module.exports = {
    create: create
};
