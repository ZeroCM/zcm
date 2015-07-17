var zcm = require('./zcm');

z = zcm.create();
z.subscribe('EXAMPLE', function(rbuf, channel) {
    console.log('Got Message on channel "'+channel+'"');
    z.publish('FOOBAR', "HI");
});
