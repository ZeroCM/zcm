var express = require('express');
var app = express();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var zcomm = require('./zcomm').create()

app.use(express.static("public"));

zcomm.subscribe('', function(channel, data) {
    var msg = {channel: channel.toString(), data: data.toString()};
    io.emit('server-to-client', msg);
});

io.on('connection', function(socket){
    socket.on('client-to-server', function(msg){
        zcomm.publish(msg.channel, msg.data);
    });
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
