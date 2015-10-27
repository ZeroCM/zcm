var express = require('express');
var app = express();
var http = require('http').Server(app);
app.use(express.static("public"));

var zcm = require('zcm');
var zcmtypes = require('./zcmtypes');
var z = zcm.create(zcmtypes, 'ipc', http);

http.listen(3000, function(){
  console.log('listening on *:3000');
});
