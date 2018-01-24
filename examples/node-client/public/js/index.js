var z;

function handle(channel, msg) {
    console.log('Got message on channel ' + channel + ": ", msg);
}

var subscriptions = [];

function subscribe(successCb) {
    console.log('Subscribing to BASIC_EXAMPLE');
    z.subscribe('BASIC_EXAMPLE', z.getZcmtypes('example_t'), handle,
                function _successCb (sub) {
                    console.log('Subscribed to BASIC_EXAMPLE. Sub id:', sub);
                    subscriptions.push({channel      : 'BASIC_EXAMPLE',
                                        subscription : sub});
                    if (successCb) successCb(true);
                });
}

function unsubscribe(successCb) {
    if (subscriptions.length == 0) {
        if (successCb) successCb(false);
        return;
    }
    var sub = subscriptions.pop();
    console.log('Unsubscribing from ' + sub.channel);
    z.unsubscribe(sub.subscription, function _successCb() {
        console.log('Unsubscribed from ' + sub.channel);
        if (successCb) successCb(true);
    });
}

function subscribe_all(successCb) {
    console.log('Subscribing to .*');
    z.subscribe(".*", null, handle,
                function _successCb (sub) {
                    console.log('Subscribed to .*');
                    subscriptions.push({channel      : '.*',
                                        subscription : sub});
                    if (successCb) successCb(true);
                });
}

function publish() {
    var msg = z.getZcmtypes('example_t');
    msg.timestamp = 1;
    msg.position = [2, 4, 6];
    msg.orientation = [0, 2, 4, 6];
    msg.num_ranges = 2;
    msg.ranges = [7, 6];
    msg.name = 'foobar string';
    msg.enabled = false;
    z.publish('FOOBAR', msg);
}

function publish2() {
    var msg = z.getZcmtypes('example_t');
    msg.timestamp = 2;
    z.publish('FOOBAR', msg);
}

function flush() {
    z.flush(function() { console.log('flushed'); });
}

function pause() {
    z.pause(function() { console.log('paused'); });
}

function resume() {
    z.resume(function() { console.log('resumed'); });
}

onload = function(){
    z = zcm.create();

    z.setQueueSize(5);
}
