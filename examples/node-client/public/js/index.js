var z;

function handle(channel, msg) {
    console.log('Got message on channel ' + channel + ": ", msg);
}

var subscriptions = [];

function subscribe() {
    console.log('Subscribing to FOOBAR_SERVER');
    subscriptions.push({channel: 'FOOBAR_SERVER',
                        subscription: z.subscribe('FOOBAR_SERVER', 'example_t', handle)});
    return true;
}

function unsubscribe() {
    if (subscriptions.length == 0)
        return false;
    var sub = subscriptions.pop();
    console.log('Unsubscribing from ' + sub.channel);
    z.unsubscribe(sub.subscription);
    return true;
}

function subscribe_all() {
    console.log('Subscribing to .*');
    subscriptions.push({channel: ".*",
                        subscription: z.subscribe_all(handle)});
    return true;
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
