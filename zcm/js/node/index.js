/*******************************************************
 * NodeJS Native N-API bindings to ZCM
 * -----------------------------------
 * This replaces the previous ffi-napi implementation
 * with a native N-API addon for better performance
 ******************************************************/

const zcmNative = require("./build/Release/zcm_native");
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

/**
 * Callback that handles data received on the zcm transport which this program has subscribed to
 * @callback dispatchRawCallback
 * @param {string} channel - the zcm channel
 * @param {Buffer} data - raw data that can be decoded into a zcmtype
 */

/**
 * Callback that handles data received on the zcm transport which this program has subscribed to
 * @callback dispatchDecodedCallback
 * @param {string} channel - the zcm channel
 * @param {zcmtype} msg - a decoded zcmtype
 */

function zcm(zcmtypes, zcmurl) {
  const parent = this;

  parent.currSubId = 0;
  parent.subscriptions = {};
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

  // Create native ZCM instance
  try {
    parent.nativeZcm = new zcmNative.ZcmNative(zcmurl);
  } catch (error) {
    return null;
  }

  /**
   * Publishes a zcm message on the created transport
   * @param {string} channel - the zcm channel to publish on
   * @param {zcmtype instance} msg - the decoded message (must be a zcmtype)
   */
  zcm.prototype.publish = function (channel, msg) {
    const encodedData = parent.zcmtypeHashMap[msg.__hash].encode(msg);
    return publish_raw(channel, encodedData);
  };

  /**
   * Publishes a zcm message on the created transport
   * @param {string} channel - the zcm channel to publish on
   * @param {Buffer} data - the encoded message (use the encode function of a generated zcmtype)
   */
  function publish_raw(channel, data) {
    const buffer = Buffer.from(data);
    return parent.nativeZcm.publish(channel, buffer);
  }

  /**
   * Subscribes to a zcm channel on the created transport
   * @param {string} channel - the zcm channel to subscribe to
   * @param {string} type - the zcmtype of messages on the channel (must be a generated
   *                        type from zcmtypes.js)
   * @param {dispatchDecodedCallback} cb - callback to handle received messages
   * @param {successCb} successCb - callback for successful subscription
   */
  zcm.prototype.subscribe = function (channel, _type, cb, successCb) {
    if (_type) {
      // Note: this lookup is because the type that is given by a client doesn't have
      //       the necessary functions, so we need to look up our complete class here
      var hash = bigint.isInstance(_type.__hash)
        ? _type.__hash.toString()
        : _type.__hash;
      var type = parent.zcmtypeHashMap[hash];
      var sub = subscribe_raw(
        channel,
        function (channel, data) {
          var msg = type.decode(data);
          if (msg != null) cb(channel, msg);
        },
        successCb,
      );
    } else {
      var sub = subscribe_raw(channel, cb, successCb);
    }
  };

  /**
   * Subscribes to a zcm channel on the created transport
   * @param {string} channel - the zcm channel to subscribe to
   * @param {dispatchRawCallback} cb - callback to handle received messages
   * @param {successCb} successCb - callback for successful subscription
   */
  function subscribe_raw(channel, cb, successCb) {
    if (!successCb)
      assert(false, "subscribe requires a success callback to be specified");

    try {
      const subId = parent.nativeZcm.subscribe(
        channel,
        function (channel, data) {
          // Convert Buffer to array for compatibility
          const dataArray = Array.from(data);
          cb(channel, dataArray);
        },
      );

      const subscription = {
        id: subId,
        nativeId: subId,
      };

      parent.subscriptions[subId] = subscription;
      successCb(subscription);
    } catch (error) {
      throw error;
    }
  }

  /**
   * Unsubscribes from the zcm channel referenced by the given subscription
   * @param {subscriptionRef} subscription - ref to the subscription to be unsubscribed from
   * @param {successCb} successCb - callback for successful unsubscription
   */
  zcm.prototype.unsubscribe = function (sub, successCb) {
    if (!(sub.id in parent.subscriptions)) return;

    setTimeout(function unsub() {
      try {
        var ret = parent.nativeZcm.unsubscribe(sub.nativeId);
        if (ret == exports.ZCM_EOK) {
          delete parent.subscriptions[sub.id];
          if (successCb) successCb();
        } else if (ret == exports.ZCM_EAGAIN) {
          setTimeout(unsub, 0);
        } else {
          throw new Error("Unsubscribe failed with return code: " + ret);
        }
      } catch (error) {
        throw error;
      }
    }, 0);
  };

  /**
   * Forces all incoming and outgoing messages to be flushed to their handlers / to the transport.
   * @params {doneCb} doneCb - callback for successful flush
   */
  zcm.prototype.flush = function (doneCb) {
    setTimeout(function f() {
      try {
        var ret = parent.nativeZcm.flush();
        if (ret == exports.ZCM_EOK) {
          if (doneCb) doneCb();
        } else if (ret == exports.ZCM_EAGAIN) {
          setTimeout(f, 0);
        } else {
          throw new Error("Flush failed with return code: " + ret);
        }
      } catch (error) {
        throw error;
      }
    }, 0);
  };

  /**
   * Starts the zcm internal threads. Called by default on creation
   */
  zcm.prototype.start = function () {
    parent.nativeZcm.start();
  };

  /**
   * Stops the zcm internal threads.
   * @params {stoppedCb} stoppedCb - callback for successful stop
   */
  zcm.prototype.stop = function (stoppedCb) {
    setTimeout(function s() {
      try {
        var ret = parent.nativeZcm.stop();
        if (ret == exports.ZCM_EOK) {
          if (stoppedCb) stoppedCb();
        } else if (ret == exports.ZCM_EAGAIN) {
          setTimeout(s, 0);
        } else {
          throw new Error("Stop failed with return code: " + ret);
        }
      } catch (error) {
        throw error;
      }
    }, 0);
  };

  /**
   * Pauses transport publishing and message dispatch
   */
  zcm.prototype.pause = function () {
    parent.nativeZcm.pause();
  };

  /**
   * Resumes transport publishing and message dispatch
   */
  zcm.prototype.resume = function () {
    parent.nativeZcm.resume();
  };

  /**
   * Sets the recv and send queue sizes within zcm
   */
  zcm.prototype.setQueueSize = function (sz, cb) {
    setTimeout(function s() {
      try {
        var ret = parent.nativeZcm.setQueueSize(sz);
        if (ret == exports.ZCM_EOK) {
          if (cb) cb();
        } else if (ret == exports.ZCM_EAGAIN) {
          setTimeout(s, 0);
        } else {
          throw new Error("Set queue size failed with return code: " + ret);
        }
      } catch (error) {
        throw error;
      }
    }, 0);
  };

  /**
   * Writes the topology index file showing what channels were published and received
   */
  zcm.prototype.writeTopology = function (name) {
    return parent.nativeZcm.writeTopology(name);
  };

  zcm.prototype.destroy = function () {
    if (parent.nativeZcm) {
      parent.nativeZcm.destroy();
      parent.nativeZcm = null;
    }
  };

  parent.start();
}

function zcm_create(zcmtypes, zcmurl, http, socketIoOptions = {}) {
  var ret = new zcm(zcmtypes, zcmurl);

  if (http) {
    var io = require("socket.io")(http, { ...socketIoOptions, path: "/zcm" });

    io.on("connection", function (socket) {
      var subscriptions = {};
      var nextSub = 0;
      socket.on("client-to-server", function (data) {
        ret.publish(data.channel, data.msg);
      });
      socket.on("subscribe", function (data, returnSubscription) {
        var subId = nextSub++;
        ret.subscribe(
          data.channel,
          data.type,
          function (channel, msg) {
            socket.emit("server-to-client", {
              channel: channel,
              msg: msg,
              subId: subId,
            });
          },
          function successCb(subscription) {
            subscriptions[subId] = subscription;
            returnSubscription(subId);
          },
        );
      });
      socket.on("unsubscribe", function (subId, successCb) {
        if (!(subId in subscriptions)) {
          successCb();
          return;
        }
        ret.unsubscribe(subscriptions[subId], function _successCb() {
          delete subscriptions[subId];
          if (successCb) successCb();
        });
      });
      socket.on("flush", function (doneCb) {
        ret.flush(doneCb);
      });
      socket.on("pause", function (cb) {
        ret.pause();
        if (cb) cb();
      });
      socket.on("resume", function (cb) {
        ret.resume();
        if (cb) cb();
      });
      socket.on("setQueueSize", function (sz, cb) {
        ret.setQueueSize(sz, cb);
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
