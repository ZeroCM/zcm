var zcm = require('zcm');
var express = require('express');
var app = express();
var http = require('http').Server(app);

var zcmtypes = require('./zcmtypes');
var example_t = zcmtypes.example_t;
var multidim_t = zcmtypes.multidim_t;

app.use(express.static("public"));

var zIPC = zcm.create('ipc', http);
var zWebSock = zcm.create('websock', http);

zIPC.subscribe('EXAMPLE', function(channel, data) {
    zWebSock.publish(channel, example_t.decode(data));
});

zWebSock.subscribe('FOOBAR', function(channel, msg) {
    zIPC.publish(channel, example_t.encode(msg));
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
