/*******************************************************
 * NodeJS FFI bindings to ZCM
 * --------------------------
 * Could/Should be more efficent, but it should
 * suffice in the short-term
 ******************************************************/
var ffi = require('ffi');

var libzcm = new ffi.Library('libzcm', {
    'zcm_create':     ['pointer', []],
    'zcm_destroy':    ['void', ['pointer']],
    'zcm_publish':    ['int', ['pointer', 'string', 'string', 'int']],
    'zcm_subscribe':  ['int', ['pointer', 'string', 'pointer', 'pointer']],
    'zcm_handle':     ['int', ['pointer']],
    'zcm_poll':       ['int', ['pointer', 'int']],
})
libzcm.zcm_handle_async = function(z) {
    libzcm.zcm_handle.async(z, function(err, res) {
        libzcm.zcm_handle_async(z);
    });
}

exports.create = function() {
    var z = libzcm.zcm_create();

    function publish(channel, data) {
        libzcm.zcm_publish.async(z, channel, data, data.length, function(err, res){});
    }

    function subscribe(channel, cb) {
        var funcPtr = ffi.Callback('void', ['pointer', 'string', 'pointer'], cb);
        process.on('exit', function() { funcPtr;}); // Force an extra ref to avoid Garbage Collection
        libzcm.zcm_subscribe(z, channel, funcPtr, null);
    }

    libzcm.zcm_handle_async(z);

    return {
        publish: publish,
        subscribe: subscribe
    };
}
