var zcm = require('zcm');
var zcmtypes = require('./zcmtypes');

var example_t = zcmtypes.example_t;
var multidim_t = zcmtypes.multidim_t;

z = zcm.create();
z.subscribe('EXAMPLE', function(data, channel) {
    console.log('Got Message on channel "'+channel+'"');
    var msg = example_t.decode(data);
    console.log(msg);
    z.publish('FOOBAR', example_t.encode(msg));
});
