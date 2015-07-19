var zcm = require('./zcm');
var zcmtypes = require('./zcmtypes');

var Example = zcmtypes.Example;
var Msg = zcmtypes.Msg;

z = zcm.create();

z.subscribe('EXAMPLE', function(data, channel) {
    console.log('Got Message on channel "'+channel+'"');
    console.log(Example.decode(data));
    z.publish('FOOBAR', "HI");
    process.exit(0);
});

// z.subscribe('MSG', function(data, channel) {
//     console.log('Got Message on channel "'+channel+'"');
//     console.log(Msg.decode(data));
// });
