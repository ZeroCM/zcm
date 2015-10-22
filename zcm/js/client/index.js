var zcm = (function(){

    function create()
    {
        var socket = io();

        // Channel -> Callback
        var callbacks = {};

        socket.on('server-to-client', function(data){
            var chan = data.channel;
            if (chan in callbacks) {
                callbacks[chan].callback(chan, data.msg);
            }
        });

        /**
         * Publishes a message on the given channel of the specified zcmtype
         * @param {string} channel - the zcm channel to publish on
         * @param {string} type - the zcmtype of messages on the channel (must be a generated
         *                        type from zcmtypes.js)
         * @param {zcmtype} msg - a decoded zcmtype (from the generated types in zcmtypes.js)
         */
        function publish(channel, type, msg) {
            socket.emit('client-to-server', {channel: channel, type: type, msg: msg});
        }

        /**
         * Subscribes to zcm messages on the given channel of the specified zcmtype.
         * TODO: Currently, the js implementation can only support one subscription per channel
         * @param {string} channel - the zcm channel to subscribe to
         * @param {string} type - the zcmtype of messages on the channel (must be a generated
         *                        type from zcmtypes.js)
         * @param {dispatchCallback} cb - handler for received messages
         */
        function subscribe(channel, type, cb) {
            socket.emit("subscribe", {channel: channel, type: type},
                        function(subscription) {
                callbacks[channel].subscription = subscription;
            });
            // change this so that it can support multiple channels
            callbacks[channel] = { callback: cb, subscription: null, };
        }

        /**
         * Unsubscribes from the zcm messages on the given channel
         * TODO: Currently, the js implementation can only support one subscription per channel
         * @param {string} channel - the zcm channel to unsubscribe from
         */
        function unsubscribe(channel) {
            if (channel in callbacks) {
                var sub = callbacks[channel].subscription;
                if (sub != null) {
                    socket.emit("unsubscribe", sub);
                    return;
                }
            }
            console.log("No subscription found, cannot unsubscribe");
        }

        return {
            publish:     publish,
            subscribe:   subscribe,
            unsubscribe: unsubscribe,
        };
    }

    return {
        create: create
    };
})();
