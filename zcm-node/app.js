var zcm = require('./zcm');
var zcmtypes = require('./zcmtypes');

var Example = zcmtypes.Example;
var Msg = zcmtypes.Msg;

z = zcm.create();

z.subscribe('EXAMPLE', function(data, channel) {
    console.log('Got Message on channel "'+channel+'"');
    var msg = Example.decode(data);
    console.log(msg);
    var newdata = Example.encode(msg);
    console.log(data);
    console.log(newdata);
    var newmsg = Example.decode(newdata);
    console.log(newmsg);
    z.publish('FOOBAR', newdata);
    //process.exit(0);
});

// z.subscribe('MSG', function(data, channel) {
//     console.log('Got Message on channel "'+channel+'"');
//     console.log(Msg.decode(data));
// });
