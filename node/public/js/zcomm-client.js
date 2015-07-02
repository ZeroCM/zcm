var zcomm = (function(){

    function create()
    {
        var socket = io();

        var callback = null;
        socket.on('server-to-client', function(msg){
            if (callback)
                callback(msg.channel, msg.data);
        });

        return {
            subscribe: function(channel, cb) {
                callback = cb
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
