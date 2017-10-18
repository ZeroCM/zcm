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

setInterval(function() {
    z.publish("FOOBAR_SERVER", "example_t", {
        timestamp: 0,
        position: [2, 4, 6],
        orientation: [0, 2, 4, 6],
        num_ranges: 2,
        ranges: [7, 6],
        name: 'foobar string',
        enabled: false,
    });
}, 1000);

var sub = null;
z.subscribe(".*", null, function(channel, msg) {
    console.log("Subscribe message received on channel " + channel);
}, function successCb (_sub) {
    sub = _sub;
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});

process.on('exit', function() {
    z.unsubscribe(sub);
});
