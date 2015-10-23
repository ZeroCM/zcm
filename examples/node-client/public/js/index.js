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
    z.subscribe('EXAMPLE', 'example_t', handleExample);
    return true;
}

function unsubscribe() {
    console.log('Unsubscribing from EXAMPLE');
    z.unsubscribe('EXAMPLE');
    return true;
}

onload = function(){
    z = zcm.create()
}
