var express = require('express');
var app = express();
var http = require('http').Server(app);
app.use(express.static("public"));

var zcm = require('zerocm');
var zcmtypes = require('zcmtypes');
var z = zcm.create(zcmtypes, null, http);
if (!z) {
    throw "Failed to create ZCM";
}

function basicExample()
{
    setInterval(function() {
        var msg = new zcmtypes.example_t();
        msg.timestamp   = 0;
        msg.position    = [2, 4, 6];
        msg.orientation = [0, 2, 4, 6];
        msg.num_ranges  = 2;
        msg.ranges      = [7, 6];
        msg.name        = 'foobar string';
        msg.enabled     = false;
        var r = new zcmtypes.recursive_t();
        r.e = msg;
        z.publish("FOOBAR_SERVER", msg);
    }, 1000);
}

function multidimExample()
{
    setInterval(function() {
        var msg = new zcmtypes.multidim_t();
        msg.rows = 2;
        msg.jk = 2;
        msg.mat = [
                   [
                    [1, 2],
                    [3, 4]
                   ],
                   [
                    [5, 6],
                    [7, 8]
                   ]
                  ];
        z.publish("MULTIDIM_EXAMPLE", msg);
    }, 1000);
}

function packageExample()
{
    var state = false;
    setInterval(function() {
        var msg = new zcmtypes.test_package.packaged_t();
        msg.packaged = state;
        state = !state;
        z.publish("PACKAGE_EXAMPLE", msg);
    }, 1000);
}

basicExample();
multidimExample();
packageExample();
// RRR: test recursive type here

// RRR: test a typed subscription here

var sub = null;
z.subscribe(".*", null, function(channel, msg) {
    console.log("Subscribe message received on channel " + channel);
}, function successCb (_sub) {
    sub = _sub;
});

process.on('exit', function() {
    if (sub) z.unsubscribe(sub);
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
