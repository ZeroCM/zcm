/////////////////// ZCM Interface /////////////////////
var zcm = require('zcm');
var zcmtypes = require('./zcmtypes');
var example_t = zcmtypes.example_t;
var multidim_t = zcmtypes.multidim_t;

z = zcm.create();
z.subscribe('EXAMPLE', function(data, channel) {
    console.log('Got Message on channel "'+channel+'"');
    var msg = example_t.decode(data);
    console.log(msg);
    z.publish('FOOBAR', example_t.encode(msg));
});

//////////////////// Web Server ///////////////////////
var http = require('http');
var fs = require('fs');

var statics = {}
function addStatic(fname, type) {
    statics['/'+fname] = {
        data: fs.readFileSync('public/'+fname),
        type: type,
    };
}
addStatic('index.html', 'html');
addStatic('js/index.js', 'javascript');
addStatic('js/zcm-client.js', 'javascript');
addStatic('css/style.css', 'css');

var server = http.createServer(function(req, res){
    if (req.url == '/')
        req.url = '/index.html'
    var st = statics[req.url];
    if (st) {
      res.writeHead(200, { 'Content-Type': 'text/' + st.type });
      res.end(st.data);
    } else {
        console.log("Unknown static url: '"+req.url+"'");
    }
});

server.listen(3000);
