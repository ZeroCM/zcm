var z;

function handleExample(channel, msg) {
    console.log('Got EXAMPLE: ', msg);
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

function subscribe() {
    console.log('Subscribing to EXAMPLE');
    // RRR: we actually CAN subscribe to the channel multiple times now, so maybe drop the
    //      subId's into a map or something and have unsubscribe just unsub from the first one
    z.subscribe('EXAMPLE', 'example_t', handleExample);
    return true;
}

function unsubscribe() {
    console.log('Unsubscribing from EXAMPLE');
    // RRR: due to the changes made to subscribe / unsubscribe, the following no longer works,
    //      it needs to be changed to use an id that is returned from subscribe
    z.unsubscribe('EXAMPLE');
    return true;
}

// RRR: would be nice to see a test of subscribe all in here

onload = function(){
    z = zcm.create()
}
