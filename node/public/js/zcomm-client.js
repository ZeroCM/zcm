var zcomm = (function(){

    function create()
    {
        var socket = io();

        // Channel -> Callback
        var callbacks = {};

        socket.on('server-to-client', function(msg){
            var chan = msg.channel;
            if (chan in callbacks)
                callbacks[chan](chan, msg.data);
        });

        return {
            subscribe: function(channel, cb) {
                callbacks[channel] = cb;
            },
            publish: function(channel, data) {
                var msg = {channel: channel, data: data};
                socket.emit('client-to-server', msg);
            }
        };
    }

    return {
        create: create
    };

})();
