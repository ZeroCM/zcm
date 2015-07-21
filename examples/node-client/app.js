var express = require('express');
var app = express();
var http = require('http').Server(app);
app.use(express.static("public"));

var zcm = require('zcm');
var zcmtypes = require('./zcmtypes');
zcm.connect_client(http, zcmtypes, {
    'EXAMPLE': 'example_t',
    'FOOBAR':  'example_t',
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
