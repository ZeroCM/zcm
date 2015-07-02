var zmq = require('zmq');

var PUB_ADDR = 'ipc:///tmp/pub';
var SUB_ADDR = 'ipc:///tmp/sub';

function create() {
    var sub = zmq.socket('sub');
    sub.connect(SUB_ADDR);

    var pub = zmq.socket('pub');
    pub.connect(PUB_ADDR);

    var callback = null;

    sub.on('message', function(channel, data) {
        if (callback)
            callback(channel, data);
    });

    return {
        subscribe: function(channel, cb) {
            sub.subscribe(channel);
            callback = cb;
        },
        publish: function(channel, data) {
            pub.send([channel, data]);
        }
    };
}

module.exports = {
    create: create
};
