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
var voidRef = ref.refType('void')
var charRef = ref.refType('char')
var recvBuf = Struct({
    // Note: it is VERY important that this struct match the zcm_recv_buf_t struct in zcm.h
    data:  charRef,
    len:   ref.types.uint32,
    utime: ref.types.uint64,
    zcm:   voidRef,
});
var recvBufRef = ref.refType(recvBuf);

var libzcm = new ffi.Library('libzcm', {
    'zcm_create':     ['pointer', ['string']],
    'zcm_destroy':    ['void', ['pointer']],
    'zcm_publish':    ['int', ['pointer', 'string', 'pointer', 'int']],
    'zcm_subscribe':  ['int', ['pointer', 'string', 'pointer', 'pointer']],
    'zcm_start':      ['void', ['pointer']],
    'zcm_stop':       ['void', ['pointer']],
});

function makeDispatcher(cb)
{
    return function(rbuf, channel, usr) {
        // XXX This decoder makes a LOT of assumptions about the underlying machine
        //     These are not all correct for archs other than x86-64
        // Note: it is VERY important that this struct is decoded to match the zcm_recv_buf_t
        //       struct in zcm.h
        var data   = ref.readPointer(rbuf, 0);
        // Note: despite len being a 32 bit uint, the following seems to work (simply inspected
        //       by printing the interpreted utime, and I suspect it is due to the internal
        //       workings of FFI, might be worth verifying though
        var len    = ref.readUInt64LE(rbuf, 8);
        var utime  = ref.readUInt64LE(rbuf, 16);
        var zcmPtr = ref.readPointer(rbuf, 24);
        var dataBuf = ref.reinterpret(data, len);
        cb(channel, new Buffer(dataBuf));
    }
}

function libzcmTransport(transport) {
    var z = libzcm.zcm_create(transport);
    if (z.isNull()) {
        console.log("Err: Failed to create transport '"+transport+"'");
        return {};
    }

    libzcm.zcm_start(z);

    function publish(channel, data) {
        libzcm.zcm_publish.async(z, channel, data, data.length, function(err, res){});
    }

    function subscribe(channel, cb) {
        var funcPtr = ffi.Callback('void', [recvBufRef, 'string', 'pointer'], makeDispatcher(cb));
        process.on('exit', function() { funcPtr; }); // Force an extra ref to avoid Garbage Collection
        libzcm.zcm_subscribe(z, channel, funcPtr, null);
    }

    return {
        publish: publish,
        subscribe: subscribe
    };
}

function zcm_create(http, zcmtypes, zcmurl)
{
    zcmurl = zcmurl || "ipc";

    var io = require('socket.io')(http);
    var zcmServer = libzcmTransport(zcmurl);

    // Channel -> Callback
    var callbacks = {};

    function publish(channel, msg) {
        io.emit('server-to-client', {
            channel: channel,
            msg: msg
        });
    }

    io.on('connection', function(socket){
        socket.on('client-to-server', function(data){
            var channel = data.channel;
            var typename = data.type;
            var type = zcmtypes[typename];
            var msg = data.msg;
            zcmServer.publish(channel, type.encode(msg));
        });
        socket.on('subscribe', function(data){
            var subChannel = data.channel;
            var subTypename = data.type;
            var subType = zcmtypes[subTypename];
            zcmServer.subscribe(data.channel, function(channel, data){
                publish(channel, subType.decode(data));
            });
        });
    });

    return io;
}

exports.create = zcm_create;
