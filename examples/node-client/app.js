var express = require('express');
var app = express();
var http = require('http').Server(app);
var assert = require('assert');
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
        msg.nExamples1  = 0;
        msg.nExamples2  = 0;
        msg.subExamples = [];
        msg.subStrings  = [];
        z.publish("BASIC_EXAMPLE", msg);
    }, 1000);
}

function recursiveExample()
{
    setInterval(function() {
        var msg = new zcmtypes.example_t();
        // Test both methods of accessing consts
        msg.timestamp   = zcmtypes.example_t.test_const_32_max_hex;
        msg.timestamp   = msg.test_const_32_max_hex;
        msg.position    = [2, 4, 6];
        msg.orientation = [0, 2, 4, 6];
        msg.num_ranges  = 2;
        msg.ranges      = [7, 6];
        msg.name        = 'foobar string';
        msg.enabled     = false;
        msg.nExamples1  = 0;
        msg.nExamples2  = 0;
        msg.subExamples = [];
        msg.subStrings  = [];
        var r = new zcmtypes.recursive_t();
        r.e = msg;
        z.publish("RECURSIVE_EXAMPLE", r);
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
    var t = 0;
    setInterval(function() {
        var msg = new zcmtypes.test_package.packaged_t();
        msg.packaged = state;
        msg.a.packaged = !state;
        msg.a.e.timestamp = t++;
        msg.a.e.p.packaged = state;

        state = !state;

        z.publish("PACKAGE_EXAMPLE", msg);
    }, 1000);
}

function encodeExample()
{
    var encSub;

    const chan = "ENCODED_EXAMPLE";

    const enc = new zcmtypes.example_t();
    enc.timestamp   = 0;
    enc.position    = [2, 4, 6];
    enc.orientation = [0, 2, 4, 6];
    enc.num_ranges  = 2;
    enc.ranges      = [7, 6];
    enc.name        = 'foobar string';
    enc.enabled     = false;
    enc.nExamples1  = 0;
    enc.nExamples2  = 0;
    enc.subExamples = [];
    enc.subStrings  = [];

    z.subscribe(chan, zcmtypes.encoded_t, function(channel, msg){
        const recEnc = zcmtypes.example_t.decode(msg.msg);
        console.log("Encoded message received on channel " + channel + ": " + recEnc.name);
    }, function successCb(_sub) { encSub = _sub; });

    setInterval(function() {
        var msg = new zcmtypes.encoded_t();
        const buf = enc.encode();
        msg.n = buf.length;
        msg.msg = buf;
        z.publish(chan, msg);
    }, 1000);
}

basicExample();
recursiveExample();
multidimExample();
packageExample();
encodeExample();

// Intentionally not saving the subscription here to make sure we don't
// segfault due to not tracking the subscription in user-space
z.subscribe("RECURSIVE_EXAMPLE", zcmtypes.recursive_t, function(channel, msg) {
    console.log("Typed message received on channel " + channel);
    assert('e' in msg && 'timestamp' in msg.e &&
           msg.e.timestamp == zcmtypes.example_t.test_const_32_max_hex, "Wrong msg received");
}, function successCb (_sub) {
});

var typedSub2 = null;
z.subscribe("PACKAGED", zcmtypes.test_package.packaged_t, function(channel, msg) {
    console.log("Typed message received on channel " + channel);
}, function successCb (_sub) {
    typedSub2 = _sub;
});

var sub = null;
z.subscribe(".*", null, function(channel, msg) {
    console.log("Untyped message received on channel " + channel);
}, function successCb (_sub) {
    sub = _sub;
});

process.on('exit', function() {
    if (sub) z.unsubscribe(sub);
    if (typedSub2) z.unsubscribe(typedSub2);
    z.destroy();
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
