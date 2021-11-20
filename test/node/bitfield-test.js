var assert = require('assert');
var bigint = require('big-integer');

let channel = 'BITFIELD';
let numMsgs = 10;
let periodMs = 100;

function test(z, zcmtypes, doneCb) {
  let subs;
  let success;

  z.subscribe(
    channel,
    zcmtypes.bitfield_t,
    (channel, msg) => {
      if (success) return;
      if (msg.field1 != -1) success = 'Failed to decode field1';
      if (msg.field2[0][0] != -1) success = 'Failed to decode field2';
      if (msg.field2[0][1] != 0) success = 'Failed to decode field2';
      if (msg.field2[0][2] != -1) success = 'Failed to decode field2';
      if (msg.field2[0][3] != 0) success = 'Failed to decode field2';
      if (msg.field2[1][0] != -1) success = 'Failed to decode field2';
      if (msg.field2[1][1] != 0) success = 'Failed to decode field2';
      if (msg.field2[1][2] != -1) success = 'Failed to decode field2';
      if (msg.field2[1][3] != 0) success = 'Failed to decode field2';
      if (msg.field3 != -1) success = 'Failed to decode field3';
      if (msg.field4 != -3) success = 'Failed to decode field4';
      if (msg.field5 != 7) success = 'Failed to decode field5';
      if (msg.field6 != 0) success = 'Failed to decode field6';
      if (msg.field7 != 0) success = 'Failed to decode field7';
      if (msg.field8_dim1 != 0) success = 'Failed to decode field8_dim1';
      if (msg.field8_dim2 != 0) success = 'Failed to decode field8_dim2';
      if (msg.field8.length != 0) success = 'Failed to decode field8';
      if (msg.field9 != ~(1 << 27) + 1) success = 'Failed to decode field9';
      if (msg.field10.toString() != bigint(1).shiftLeft(52).or(1).toString()) {
        success = 'Failed to properly decode bitfield';
      }
      if (msg.field11 != 3) success = 'Failed to decode field11';
      for (let i = 0; i < 3; ++i) {
        for (let j = 0; j < 2; ++j) {
          for (let k = 0; k < 2; ++k) {
            for (let l = 0; l < 2; ++l) {
              if (msg.field12[i][j][k][l] != k + l + 1) {
                success = `Failed to decode field12[${i}][${j}][${k}][${l}]`;
                success = `${msg.field12[i][j][k][l]} != ${k + l + 1}`;
              }
            }
          }
        }
      }
      if (msg.field15 != -60) success = 'Failed to decode field15';
      if (msg.field16 != 2) success = 'Failed to decode field16';
      if (msg.field18 != 15) success = 'Failed to decode field18';
      if (msg.field19 != 68) success = 'Failed to decode field19';
      if (msg.field20 != 2) success = 'Failed to decode field20';
      if (msg.field22 != msg.FIELD22_TEST) 'Bad decode of field 22';
      if (msg.field23 != msg.FIELD23_TEST) 'Bad decode of field 23';
      if (msg.field24 != msg.FIELD24_TEST) 'Bad decode of field 24';
      if (msg.field25 != 3) success = 'Bad decode of field 25';
      if (msg.field26 != 255) success = 'Bad decode of field 26';
      if (msg.field27 != 3) success = 'Bad decode of field 27';
      if (msg.field28 != 0x7f) success = 'Bad decode of field 28';
      if (msg.field29 != 3) success = 'Bad decode of field 29';
      if (msg.field30 != 0x7fff) success = 'Bad decode of field 30';
      if (msg.field31 != 0xf) success = 'Bad decode of field 31';
      if (msg.field32 != 0x7fffffff) success = 'Bad decode of field 32';
      if (msg.field33 != 0xf) success = 'Bad decode of field 33';
      if (msg.field34 != 0x7fffffffffffffff) success = 'Bad decode of field 34';

      if (!success) success = 'success';
    },
    sub => {
      subs = sub;
    }
  );

  assert(zcmtypes.bitfield_t.FIELD22_TEST == 255);
  assert(zcmtypes.bitfield_t.FIELD23_TEST == 3);
  assert(zcmtypes.bitfield_t.FIELD24_TEST == 7);

  assert(zcmtypes.bitfield_t.SIGN_TEST_0 == 0x0f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_1 == -16);
  assert(zcmtypes.bitfield_t.SIGN_TEST_2 == 0x7f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_3 == -128);

  assert(zcmtypes.bitfield_t.SIGN_TEST_4 == 0x1fff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_5 == -8192);
  assert(zcmtypes.bitfield_t.SIGN_TEST_6 == 0x7fff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_7 == -32768);

  assert(zcmtypes.bitfield_t.SIGN_TEST_8 == 0x01ffffff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_9 == -33554432);
  assert(zcmtypes.bitfield_t.SIGN_TEST_10 == 0x7fffffff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_11 == -2147483648);

  assert(zcmtypes.bitfield_t.SIGN_TEST_12 == '-1');
  assert(zcmtypes.bitfield_t.SIGN_TEST_13 == '72057594037927935');
  assert(zcmtypes.bitfield_t.SIGN_TEST_14 == '-72057594037927936');
  assert(zcmtypes.bitfield_t.SIGN_TEST_15 == '9223372036854775807');
  assert(zcmtypes.bitfield_t.SIGN_TEST_16 == '-9223372036854775808');

  assert(zcmtypes.bitfield_t.SIGN_TEST_17 == 7);
  assert(zcmtypes.bitfield_t.SIGN_TEST_18 == 0x7f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_19 == 7);
  assert(zcmtypes.bitfield_t.SIGN_TEST_20 == 0x7f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_21 == 7);
  assert(zcmtypes.bitfield_t.SIGN_TEST_22 == 0x7fff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_23 == 7);
  assert(zcmtypes.bitfield_t.SIGN_TEST_24 == 0x7fffffff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_25 == 1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_26 == 7);
  assert(zcmtypes.bitfield_t.SIGN_TEST_27 == '9223372036854775807');

  assert(zcmtypes.bitfield_t.SIGN_TEST_28 == 0x7f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_29 == 0xff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_30 == 0x7f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_31 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_32 == 127);
  assert(zcmtypes.bitfield_t.SIGN_TEST_33 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_34 == 0x7fff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_35 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_36 == 32767);
  assert(zcmtypes.bitfield_t.SIGN_TEST_37 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_38 == 0x7fffffff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_39 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_40 == 2147483647);
  assert(zcmtypes.bitfield_t.SIGN_TEST_41 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_42 == '9223372036854775807');
  assert(zcmtypes.bitfield_t.SIGN_TEST_43 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_44 == '9223372036854775807');
  assert(zcmtypes.bitfield_t.SIGN_TEST_45 == -1);

  function publish() {
    var b = new zcmtypes.bitfield_t();
    b.field1 = 3;
    b.field2 = [
      [1, 0, 1, 0],
      [1, 0, 1, 0],
    ];
    b.field3 = 0xf;
    b.field4 = 5;
    b.field5 = 7;
    b.field9 = 1 << 27;
    b.field10 = bigint(1).shiftLeft(52).or(1);
    b.field11 = 3;
    for (let i = 0; i < 3; ++i) {
      for (let j = 0; j < 2; ++j) {
        for (let k = 0; k < 2; ++k) {
          for (let l = 0; l < 2; ++l) {
            b.field12[i][j][k][l] = k + l + 1;
          }
        }
      }
    }
    b.field15 = 0b1000100;
    b.field16 = 0b0000010;
    b.field18 = -1;
    b.field19 = 0b1000100;
    b.field20 = 0b0000010;
    b.field22 = b.FIELD22_TEST;
    b.field23 = b.FIELD23_TEST;
    b.field24 = b.FIELD24_TEST;
    b.field25 = 0xff;
    b.field26 = 0xff;
    b.field27 = 0x7f;
    b.field28 = 0x7f;
    b.field29 = 0x7fff;
    b.field30 = 0x7fff;
    b.field31 = 0x7fffffff;
    b.field32 = 0x7fffffff;
    b.field33 = bigint('7fffffffffffffff', 16);
    b.field34 = bigint('7fffffffffffffff', 16);

    z.publish(channel, b);

    numMsgs--;
    if (numMsgs > 0) {
      setTimeout(publish, periodMs);
    } else {
      if (subs) z.unsubscribe(subs);
      return doneCb(success === 'success' ? null : success);
    }
  }
  publish();
}

module.exports = (z, zcmtypes) => {
  return new Promise((resolve, reject) => {
    test(z, zcmtypes, err => {
      if (err) return reject(err);
      else return resolve();
    });
  });
};
