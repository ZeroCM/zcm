var z;

function handle(channel, msg) {
    console.log('Got message on channel ' + channel + ": ", msg);
}

var subscriptions = [];

function subscribe(successCb) {
    console.log('Subscribing to FOOBAR_SERVER');
    z.subscribe('FOOBAR_SERVER', 'example_t', handle,
                function _successCb (sub) {
                    console.log('Subscribed to FOOBAR_SERVER. Sub id:', sub);
                    subscriptions.push({channel      : 'FOOBAR_SERVER',
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
    z.subscribe_all(handle,
                    function _successCb (sub) {
                        console.log('Subscribed to .*');
                        subscriptions.push({channel      : '.*',
                                            subscription : sub});
                        if (successCb) successCb(true);
                    });
}

function publish() {
    z.publish('FOOBAR', 'example_t', {
        timestamp: 0,
        position: [2, 4, 6],
        orientation: [0, 2, 4, 6],
        num_ranges: 2,
        ranges: [7, 6],
        name: 'foobar string',
        enabled: false,
    });
}

onload = function(){
    z = zcm.create();
}
