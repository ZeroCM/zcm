var assert = require('assert');

let channel = 'EXAMPLE';
let numMsgs = 100;
let totalMsgs = 100;
let currentMsg = 0;
let periodMs = 0;

function test(z, zcmtypes, doneCb) {
  let subs;
  let success;
  let testCompleted = false;
  let msgReceived = 0;

  z.subscribe(
    channel,
    zcmtypes.example_t,
    (channel, msg) => {
      msgReceived++;
      console.log(`Received message ${msgReceived}`);
      if (success || testCompleted) return;

      if (msg.utime.toString() !== '10') success = 'Bad decode of utime';
      if (msg.position[0] !== 1) success = 'Bad decode of position[0]';
      if (msg.position[1] !== 2) success = 'Bad decode of position[1]';
      if (msg.position[2] !== 3.5) success = 'Bad decode of position[2]';
      if (msg.orientation[0] !== 1.1) success = 'Bad decode of orientation[0]';
      if (msg.orientation[1] !== 2.2) success = 'Bad decode of orientation[1]';
      if (msg.orientation[2] !== 3.3) success = 'Bad decode of orientation[2]';
      if (msg.orientation[3] !== 4.4) success = 'Bad decode of orientation[3]';
      if (msg.num_ranges !== 2) success = 'Bad decode of num_ranges';
      if (msg.num_ranges !== msg.ranges.length) success = 'Bad decode of num_ranges';
      if (msg.ranges[0] !== 10) success = 'Bad decode of ranges';
      if (msg.ranges[1] !== 20) success = 'Bad decode of ranges';
      if (msg.name !== 'this is a test') success = 'Bad decode of name';
      if (msg.enabled !== true) success = 'Bad decode of enabled';

      if (!success) success = 'success';
    },
    (err, sub) => {
      if (err) throw "Failed to subscribe";
      subs = sub;
    }
  );

  assert(zcmtypes.example_t.test_const_8_max_hex === -1);
  assert(zcmtypes.example_t.test_const_16_max_hex === -1);
  assert(zcmtypes.example_t.test_const_32_max_hex === -1);
  assert(zcmtypes.example_t.test_const_64_max_hex === '-1');

  assert(zcmtypes.example_t.test_const_float === 1e-20);
  assert(zcmtypes.example_t.test_const_double === 12.1e200);

  function publish() {
    currentMsg++;
    console.log(`Publishing message ${currentMsg} of ${totalMsgs}`);
    var msg = new zcmtypes.example_t();

    msg.utime = 10;
    msg.position = [1, 2, 3.5];
    msg.orientation = [1.1, 2.2, 3.3, 4.4];
    msg.ranges = [10, 20];
    msg.num_ranges = msg.ranges.length;
    msg.name = 'this is a test';
    msg.enabled = true;

    z.publish(channel, msg);

    // Test z.flush() method
    z.flush(err => {
      if (err) {
        console.error('z.flush() failed:', err);
        if (subs) z.unsubscribe(subs, err => {
          if (!err) return;
          console.error("Failed to unsubscribe");
        });
        return doneCb('z.flush() failed: ' + err.message);
      }

      numMsgs--;
      if (numMsgs > 0) {
        setTimeout(publish, periodMs);
      } else {
        testCompleted = true;
        if (subs) z.unsubscribe(subs, err => {
          if (!err) return;
          console.error("Failed to unsubscribe");
        });
        return doneCb(success === 'success' ? null : success);
      }
    });
  }
  setTimeout(publish, 1000);
}

module.exports = (z, zcmtypes) => {
  return new Promise((resolve, reject) => {
    test(z, zcmtypes, err => {
      if (err) return reject(err);
      else return resolve();
    });
  });
};
