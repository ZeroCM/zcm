var zcm = (function(){

    function create()
    {
        var socket = io();

        var subIds = 0;

        // Channel -> Callback
        var callbacks = {};

        socket.on('server-to-client', function(data){
            var subId = data.id;
            if (subId in callbacks) {
                callbacks[subId].callback(data.channel, data.msg);
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
         * RRR: because callbacks is now indexed by an id, we actually can support more
         *      than 1 subscription per channel now, so this TODO is done
         * TODO: Currently, the js implementation can only support one subscription per channel
         * @param {string} channel - the zcm channel to subscribe to
         * @param {string} type - the zcmtype of messages on the channel (must be a generated
         *                        type from zcmtypes.js)
         * @param {dispatchDecodedCallback} cb - handler for received messages
         */
        function subscribe(channel, type, cb) {
            var subId = subIds++;
            callbacks[subId] = { callback: cb };
            socket.emit("subscribe", {channel: channel, type: type, subId: subId},
                        function(subscription) {
                callbacks[subId].subscription = subscription;
            });
        }

        /**
         * Subscribes to all zcm messages.
         * RRR: can delete this todo, see above
         * TODO: Currently, the js implementation can only support one subscription per channel
         * @param {dispatchDecodedCallback} cb - handler for received messages
         */
        function subscribe_all(cb) {
            var subId = subIds++;
            callbacks[subId] = { callback: cb };
            socket.emit("subscribe_all", {subId: subId}, function(subscription) {
                callbacks[subId].subscription = subscription;
            });
        }

        /**
         * Unsubscribes from the zcm messages on the given channel
         * RRR: can delete this todo, see above
         * TODO: Currently, the js implementation can only support one subscription per channel
         * @param {string} channel - the zcm channel to unsubscribe from
         */
        // RRR: because callbacks is now indexed by an id instead of by the actual channel string,
        //      this function is no longer right. We need to actually pass in the subId, which
        //      means that the subscribe functions need to return it to user land. Please change
        //      the commented function descriptions too when you make the change
        function unsubscribe(channel) {
            if (channel in callbacks) {
                var sub = callbacks[channel].subscription;
                if (sub != null) { // loose compare here because sub might be undefined
                    socket.emit("unsubscribe", sub);
                    delete callbacks[channel];
                    return;
                }
            }
            console.log("No subscription found, cannot unsubscribe");
        }

        return {
            publish:        publish,
            subscribe:      subscribe,
            subscribe_all:  subscribe_all,
            unsubscribe:    unsubscribe,
        };
    }

    return {
        create: create
    };
})();
