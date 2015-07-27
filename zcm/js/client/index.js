var zcm = (function(){

    function create()
    {
        var socket = io();

        // Channel -> Callback
        var callbacks = {};

        socket.on('server-to-client', function(data){
            var chan = data.channel;
            if (chan in callbacks)
                callbacks[chan](chan, data.msg);
        });

        return {
            subscribe: function(channel, cb) {
                callbacks[channel] = cb;
            },
            publish: function(channel, msg) {
                var data = {channel: channel, msg: msg};
                socket.emit('client-to-server', data);
            }
        };
    }

    return {
        create: create
    };
})();
