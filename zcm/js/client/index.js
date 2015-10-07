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
            subscribe: function(channel, type, cb) {
                socket.emit("subscribe", {channel: channel, type: type});
                callbacks[channel] = cb;
            },
            publish: function(channel, type, msg) {
                var data = {channel: channel, type: type, msg: msg};
                socket.emit('client-to-server', data);
            }
        };
    }

    return {
        create: create
    };
})();
