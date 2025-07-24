/*******************************************************
 * NodeJS Native N-API bindings to ZCM
 * -----------------------------------
 *******************************************************/
const zcmNative =
  process.env.NODE_ENV === "dev"
    ? require("./build/Debug/zcm_native")
    : require("./build/Release/zcm_native");
const bigint = require("big-integer");
const assert = require("assert");

// Export ZCM return codes
exports.ZCM_EOK = zcmNative.ZCM_EOK;
exports.ZCM_EINVALID = zcmNative.ZCM_EINVALID;
exports.ZCM_EAGAIN = zcmNative.ZCM_EAGAIN;
exports.ZCM_ECONNECT = zcmNative.ZCM_ECONNECT;
exports.ZCM_EINTR = zcmNative.ZCM_EINTR;
exports.ZCM_EUNKNOWN = zcmNative.ZCM_EUNKNOWN;
exports.ZCM_EMEMORY = zcmNative.ZCM_EMEMORY;
exports.ZCM_EUNIMPL = zcmNative.ZCM_EUNIMPL;
exports.ZCM_NUM_RETURN_CODES = zcmNative.ZCM_NUM_RETURN_CODES;

function makePromiseApi(fn) {
  return (...args) => {
    const cb = typeof args[args.length - 1] === "function" ? args.pop() : null;
    let promRes, promRej;
    fn(...args, (err, ...resolveArgs) => {
      if (cb) cb(err, ...resolveArgs);
      if (promRes) {
        if (err) promRej(...resolveArgs);
        else promRes(...resolveArgs);
      }
    });
    return {
      promise() {
        return new Promise((res, rej) => {
          promRes = res;
          promRej = rej;
        });
      },
    };
  };
}

function zcm(zcmtypes, zcmurl) {
  const parent = this;

  // Create native ZCM instance
  try {
    parent.nativeZcm = new zcmNative.ZcmNative(zcmurl);
  } catch (error) {
    return null;
  }
  parent.zcmtypeHashMap = {};

  // Note: recursive to handle packages (which are set as objects in the zcmtypes exports)
  function rehashTypes(zcmtypes) {
    for (var type in zcmtypes) {
      if (type == "getZcmtypes") continue;
      if (typeof zcmtypes[type] == "object") {
        rehashTypes(zcmtypes[type]);
        continue;
      }
      parent.zcmtypeHashMap[zcmtypes[type].__get_hash_recursive()] =
        zcmtypes[type];
    }
  }
  rehashTypes(zcmtypes);

  /**
   * Publishes a zcm message on the created transport
   * @param {string} channel - the zcm channel to publish on
   * @param {zcmtype instance} msg - the decoded message (must be a zcmtype)
   */
  zcm.prototype.publish = function (channel, msg) {
    const encodedData = parent.zcmtypeHashMap[msg.__hash].encode(msg);
    return parent.nativeZcm.publish(channel, encodedData);
  };

  /**
   * Subscribes to a zcm channel on the created transport
   * @param {string} channel - the zcm channel to subscribe to
   * @param {string} type - the zcmtype of messages on the channel (must be a generated
   *                        type from zcmtypes.js)
   * @param {fn(channel, decodedData || encodedData)} cb - callback to handle received messages
   * @param {fn(err, subscriptionRef)} successCb - callback for successful subscription
   */
  zcm.prototype.subscribe = function (channel, _type, cb, successCb) {
    var raw_cb = cb;
    if (_type) {
      // Note: this lookup is because the type that is given by a client doesn't have
      //       the necessary functions, so we need to look up our complete class here
      var hash = bigint.isInstance(_type.__hash)
        ? _type.__hash.toString()
        : _type.__hash;
      var type = parent.zcmtypeHashMap[hash];
      raw_cb = function (channel, data) {
        var msg = type.decode(data);
        if (msg != null) cb(channel, msg);
      };
    }
    parent.nativeZcm.subscribe(
      channel,
      (...args) => {
        try {
          raw_cb(...args);
        } catch (err) {
          setImmediate(() => {
            throw err;
          }, 0);
        }
      },
      (...args) => {
        if (successCb) successCb(...args);
      },
    );
  };

  /**
   * Unsubscribes from the zcm channel referenced by the given subscription
   * @param {subscriptionRef} subscription - ref to the subscription to be unsubscribed from
   * @param {fn(err)} successCb - callback for successful unsubscription
   */
  function _unsubscribe(...args) {
    parent.nativeZcm.unsubscribe(...args);
  }
  zcm.prototype.unsubscribe = makePromiseApi(_unsubscribe);

  /**
   * Forces all incoming and outgoing messages to be flushed to their handlers / to the transport.
   * @param {fn(err)} doneCb - callback for successful flush
   */
  function _flush(...args) {
    parent.nativeZcm.flush(...args);
  }
  zcm.prototype.flush = makePromiseApi(_flush);

  /**
   * Starts the zcm internal threads. Called by default on creation
   */
  zcm.prototype.start = (...args) => {
    parent.nativeZcm.start(...args);
  };

  /**
   * Stops the zcm internal threads.
   * @param {fn(err)} successCb - callback for successful stop
   */
  function _stop(...args) {
    parent.nativeZcm.stop(...args);
  }
  zcm.prototype.stop = makePromiseApi(_stop);

  /**
   * Pauses transport publishing and message dispatch
   * @param {fn(err)} successCb - callback for successful pause
   */
  function _pause(...args) {
    parent.nativeZcm.pause(...args);
  }
  zcm.prototype.pause = makePromiseApi(_pause);

  /**
   * Resumes transport publishing and message dispatch
   * @param {fn(err)} successCb - callback for successful resume
   */
  function _resume(...args) {
    parent.nativeZcm.resume(...args);
  }
  zcm.prototype.resume = makePromiseApi(_resume);

  /**
   * Sets the recv and send queue sizes within zcm
   * @param {number} size - queue size
   * @param {fn(err)} successCb - callback for successful queue size change
   */
  function _setQueueSize(...args) {
    parent.nativeZcm.setQueueSize(...args);
  }
  zcm.prototype.setQueueSize = makePromiseApi(_setQueueSize);

  /**
   * Writes the topology index file showing what channels were published and received
   * @param {string} filename - path to file written on disk
   */
  zcm.prototype.writeTopology = function (name) {
    return parent.nativeZcm.writeTopology(name);
  };

  parent.start();
}

function zcm_create(zcmtypes, zcmurl, http, socketIoOptions = {}) {
  var ret = new zcm(zcmtypes, zcmurl);
  if (!ret.nativeZcm) return null;

  if (http) {
    var io = require("socket.io")(http, { ...socketIoOptions, path: "/zcm" });

    io.on("connection", function (socket) {
      var subscriptions = {};
      var nextSub = 0;
      socket.on("client-to-server", function (data) {
        ret.publish(data.channel, data.msg);
      });
      socket.on("subscribe", function (data, cb) {
        var subId = nextSub++;
        ret.subscribe(
          data.channel,
          data.type,
          function (channel, msg) {
            const safeMsg = Buffer.isBuffer(msg) ? Buffer.from(msg) : msg;
            socket.emit("server-to-client", {
              channel: channel,
              msg: safeMsg,
              subId: subId,
            });
          },
          function successCb(err, subscription) {
            subscriptions[subId] = subscription;
            if (cb) cb(subId);
          },
        );
      });
      socket.on("unsubscribe", function (subId, cb) {
        if (!(subId in subscriptions)) {
          if (cb) cb();
          return;
        }
        ret.unsubscribe(subscriptions[subId], function _successCb() {
          delete subscriptions[subId];
          if (cb) cb();
        });
      });
      socket.on("flush", function (cb) {
        ret.flush(cb ? cb : (err) => {});
      });
      socket.on("pause", function (cb) {
        ret.pause(cb ? cb : (err) => {});
      });
      socket.on("resume", function (cb) {
        ret.resume(cb ? cb : (err) => {});
      });
      socket.on("setQueueSize", function (sz, cb) {
        ret.setQueueSize(sz, cb ? cb : (err) => {});
      });
      socket.on("disconnect", function () {
        for (var subId in subscriptions) {
          ret.unsubscribe(subscriptions[subId]);
          delete subscriptions[subId];
        }
        nextSub = 0;
      });
      socket.emit("zcmtypes", zcmtypes.getZcmtypes());
    });
  }

  return ret;
}

exports.create = zcm_create;
