var express = require('express');
var app = express();
var http = require('http').Server(app);
app.use(express.static("public"));

var zcm = require('zcm');
var zcmtypes = require('./zcmtypes');
var z = zcm.create(zcmtypes, 'ipc', http);

var sub1;
var sub2;

function subTo1() {
    sub1 = z.subscribe("EXAMPLE", "example_t", function(channel, msg) {
        z.publish("FOOBAR_SERVER_1", "example_t", {
            timestamp: 0,
            position: [2, 4, 6],
            orientation: [0, 2, 4, 6],
            num_ranges: 2,
            ranges: [7, 6],
            name: 'foobar string',
            enabled: false,
        });
    });
}

function subTo2() {
    sub2 = z.subscribe("EXAMPLE", "example_t", function(channel, msg) {
        z.publish("FOOBAR_SERVER_2", "example_t", {
            timestamp: 0,
            position: [2, 4, 6],
            orientation: [0, 2, 4, 6],
            num_ranges: 2,
            ranges: [7, 6],
            name: 'foobar string',
            enabled: false,
        });
    });
}

setTimeout(function() { setInterval(subTo1,                              12000); },    0);
setTimeout(function() { setInterval(subTo2,                              12000); }, 3000);
setTimeout(function() { setInterval(function() { z.unsubscribe(sub1); }, 12000); }, 6000);
setTimeout(function() { setInterval(function() { z.unsubscribe(sub2); }, 12000); }, 9000);

http.listen(3000, function(){
  console.log('listening on *:3000');
});
