/*******************************************************
 * NodeJS FFI bindings to ZCM
 * --------------------------
 * Could/Should be more efficent, but it should
 * suffice in the short-term
 ******************************************************/
var ffi = require('ffi');
var ref = require('ref');
var StructType = require('ref-struct');
var ArrayType = require('ref-array');

// Define some types
var voidRef = ref.refType('void')
var charRef = ref.refType('char')

var recvBuf = StructType({
    // Note: it is VERY important that this struct match the zcm_recv_buf_t struct in zcm.h
    data:  charRef,
    len:   ref.types.uint32,
    utime: ref.types.uint64,
    zcm:   voidRef,
});
var recvBufRef = ref.refType(recvBuf);

var subscription = StructType({
    // Note: it is VERY important that this struct match the zcm_sub_t struct in zcm.h
    channel:  ArrayType(ref.types.char),
    callback: voidRef,
    usr:      voidRef,
});
var subscriptionRef = ref.refType(subscription);

// Define our Foreign Function Interface to the zcm library
var libzcm = new ffi.Library('libzcm', {
    'zcm_create':      ['pointer', ['string']],
    'zcm_destroy':     ['void', ['pointer']],
    'zcm_publish':     ['int', ['pointer', 'string', 'pointer', 'int']],
    'zcm_subscribe':   ['pointer', ['pointer', 'string', 'pointer', 'pointer']],
    'zcm_unsubscribe': ['int', ['pointer', 'pointer']],
    'zcm_start':       ['void', ['pointer']],
    'zcm_stop':        ['void', ['pointer']],
});

/**
 * Callback that handles data received on the zcm transport which this program has subscribed to
 * @callback dispatchCallback
 * @param {string} channel - the zcm channel
 * @param {zcmtype} msg - a decoded zcmtype
 */

/**
 * Creates a dispatch function that can interface with the ffi library
 * @param {dispatchCallback} cb - the js callback function to be linked into the ffi library
 */
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

// TODO: consider adding support for server driven comms (would be easy to export bindings into
//       this library for the server to use instead of the client)
function libzcmTransport(transport) {
    var z = libzcm.zcm_create(transport);
    if (z.isNull()) {
        console.log("Err: Failed to create transport '"+transport+"'");
        return {};
    }

    libzcm.zcm_start(z);

    /**
     * Publishes a zcm message on the created transport
     * @param {string} channel - the zcm channel to publish on
     * @param {Buffer} data - the encoded message (use the encode function of a generated zcmtype)
     */
    function publish(channel, data) {
        libzcm.zcm_publish.async(z, channel, data, data.length, function(err, res) {});
    }

    /**
     * Subscribes to a zcm channel on the created transport
     * @param {string} channel - the zcm channel to subscribe to
     * @param {dispatchCallback} cb - callback to handle received messages
     * @returns {subscriptionRef} reference to the subscription, used to unsubscribe
     */
    function subscribe(channel, cb) {
        var funcPtr = ffi.Callback('void', [recvBufRef, 'string', 'pointer'], makeDispatcher(cb));
        // Force an extra ref to avoid Garbage Collection
        process.on('exit', function() { funcPtr; });
        return libzcm.zcm_subscribe(z, channel, funcPtr, null);
    }

    /**
     * Unsubscribes from the zcm channel referenced by the given subscription
     * @param {subscriptionRef} subscription - ref to the subscription to be unsubscribed from
     */
    function unsubscribe(subscription) {
        libzcm.zcm_unsubscribe(z, subscription);
    }

    return {
        publish:     publish,
        subscribe:   subscribe,
        unsubscribe: unsubscribe,
    };
}

function zcm_create(http, zcmtypes, zcmurl)
{
    zcmurl = zcmurl || "ipc";

    var io = require('socket.io')(http);
    var zcmServer = libzcmTransport(zcmurl);

    io.on('connection', function(socket) {
        socket.on('client-to-server', function(data) {
            var channel = data.channel;
            var typename = data.type;
            var type = zcmtypes[typename];
            var msg = data.msg;
            zcmServer.publish(channel, type.encode(msg));
        });
        socket.on('subscribe', function(data, returnSubscription) {
            var subChannel = data.channel;
            var subTypename = data.type;
            var subType = zcmtypes[subTypename];
            var subscription = zcmServer.subscribe(data.channel, function(channel, data) {
                socket.emit('server-to-client', {
                    channel: channel,
                    msg: subType.decode(data),
                });
            });
            returnSubscription(subscription);
        });
        socket.on('unsubscribe', function(subscription) {
            zcmServer.unsubscribe(subscription);
        });
    });

    return io;
}

exports.create = zcm_create;
