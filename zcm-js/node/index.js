/*******************************************************
 * NodeJS FFI bindings to ZCM
 * --------------------------
 * Could/Should be more efficent, but it should
 * suffice in the short-term
 ******************************************************/
var ffi = require('ffi');
var ref = require('ref');
var Struct = require('ref-struct');

// Define some types
//var int = ref.types.int
//var IntArray = ArrayType(int)
var voidRef = ref.refType('void')
var charRef = ref.refType('char')
var recvBuf = Struct({
    zcm:   voidRef,
    utime: ref.types.ulonglong,
    len:   ref.types.size_t,
    data:  charRef,
});
var recvBufRef = ref.refType(recvBuf);

var libzcm = new ffi.Library('libzcm', {
    'zcm_create':     ['pointer', []],
    'zcm_destroy':    ['void', ['pointer']],
    'zcm_publish':    ['int', ['pointer', 'string', 'pointer', 'int']],
    'zcm_subscribe':  ['int', ['pointer', 'string', 'pointer', 'pointer']],
    'zcm_handle':     ['int', ['pointer']],
    'zcm_poll':       ['int', ['pointer', 'int']],
})
libzcm.zcm_handle_async = function(z) {
    libzcm.zcm_handle.async(z, function(err, res) {
        libzcm.zcm_handle_async(z);
    });
}

function makeDispatcher(cb)
{
    return function(rbuf, channel, usr) {
        // XXX This decoder makes a LOT of assumptions about the underlying machine
        //     These are not all correct for archs other than x86-64
        var zcmPtr = ref.readPointer(rbuf, 0);
        var utime  = ref.readUInt64LE(rbuf, 8);
        var len    = ref.readUInt64LE(rbuf, 16);
        var data   = ref.readPointer(rbuf, 24);
        var dataBuf = ref.reinterpret(data, len);
        cb(channel, new Buffer(dataBuf));
    }
}

function libzcmTransport(http) {
    var z = libzcm.zcm_create();

    function publish(channel, data) {
        libzcm.zcm_publish.async(z, channel, data, data.length, function(err, res){});
    }

    function subscribe(channel, cb) {
        var funcPtr = ffi.Callback('void', [recvBufRef, 'string', 'pointer'], makeDispatcher(cb));
        process.on('exit', function() { funcPtr;}); // Force an extra ref to avoid Garbage Collection
        libzcm.zcm_subscribe(z, channel, funcPtr, null);
    }

    libzcm.zcm_handle_async(z);

    return {
        publish: publish,
        subscribe: subscribe
    };
}

function socketioTransport(http) {
    var io = require('socket.io')(http);

    // Channel -> Callback
    var callbacks = {};

    io.on('connection', function(socket){
        socket.on('client-to-server', function(data){
            var channel = data.channel;
            if (channel in callbacks)
                callbacks[channel](channel, data.msg);
        });
    });

    function publish(channel, msg) {
        io.emit('server-to-client', {
            channel: channel,
            msg: msg
        });
    }

    function subscribe(channel, cb) {
        callbacks[channel] = cb;
    }

    return {
        publish: publish,
        subscribe: subscribe
    };
}

function zcm_create(http) {
    return libzcmTransport();
}

function zcm_connect_client(http, zcmtypes, table)
{
    var zIPC = libzcmTransport();
    var zWebsock = socketioTransport(http);

    for (var channel in table) {
        var typename = table[channel];
        var type = zcmtypes[typename];
        if (!type) {
            console.log("Unknown type '"+typename+"' in connect_client()");
            continue;
        }
        zIPC.subscribe(channel, function(channel, data) {
            zWebsock.publish(channel, type.decode(data));
        });
        zWebsock.subscribe(channel, function(channel, msg) {
            zIPC.publish(channel, type.encode(msg));
        });
    }
}

exports.create = zcm_create;
exports.connect_client = zcm_connect_client;
