var zcm = require('./zcm');

z = zcm.create();
z.subscribe('EXAMPLE', function(data, channel) {
    console.log('Got Message on channel "'+channel+'"');
    console.log(data);
    z.publish('FOOBAR', "HI");
});
