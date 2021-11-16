var express = require('express');
var app = express();
var http = require('http').Server(app);
var assert = require('assert');
var bigint = require('big-integer');
app.use(express.static("public"));

var zcm = require('zerocm');
var zcmtypes = require('zcmtypes');
var z = zcm.create(zcmtypes, null, http);
if (!z) {
  throw "Failed to create ZCM";
}

function basicExample()
{
  setInterval(function() {
    var msg = new zcmtypes.example_t();
    msg.timestamp   = 0;
    msg.position    = [2, 4, 6];
    msg.orientation = [0, 2, 4, 6];
    msg.num_ranges  = 2;
    msg.ranges      = [7, 6];
    msg.name        = 'foobar string';
    msg.enabled     = false;
    msg.nExamples1  = 0;
    msg.nExamples2  = 0;
    msg.subExamples = [];
    msg.subStrings  = [];
    z.publish("BASIC_EXAMPLE", msg);
  }, 1000);
}

function recursiveExample()
{
  setInterval(function() {
    var msg = new zcmtypes.example_t();
    // Test both methods of accessing consts
    msg.timestamp   = zcmtypes.example_t.test_const_32_max_hex;
    msg.timestamp   = msg.test_const_32_max_hex;
    msg.position    = [2, 4, 6];
    msg.orientation = [0, 2, 4, 6];
    msg.num_ranges  = 2;
    msg.ranges      = [7, 6];
    msg.name        = 'foobar string';
    msg.enabled     = false;
    msg.nExamples1  = 0;
    msg.nExamples2  = 0;
    msg.subExamples = [];
    msg.subStrings  = [];
    var r = new zcmtypes.recursive_t();
    r.e = msg;
    z.publish("RECURSIVE_EXAMPLE", r);
  }, 1000);
}

function multidimExample()
{
  setInterval(function() {
    var msg = new zcmtypes.multidim_t();
    msg.rows = 2;
    msg.jk = 2;
    msg.mat = [
               [
                [1, 2],
                [3, 4]
               ],
               [
                [5, 6],
                [7, 8]
               ]
              ];
    z.publish("MULTIDIM_EXAMPLE", msg);
  }, 1000);
}

function packageExample()
{
  var state = false;
  var t = 0;
  setInterval(function() {
    var msg = new zcmtypes.test_package.packaged_t();
    msg.packaged = state;
    msg.a.packaged = !state;
    msg.a.e.timestamp = t++;
    msg.a.e.p.packaged = state;

    state = !state;

    z.publish("PACKAGE_EXAMPLE", msg);
  }, 1000);
}

function encodeExample()
{
  var encSub;

  const chan = "ENCODED_EXAMPLE";

  const enc = new zcmtypes.example_t();
  enc.timestamp   = 0;
  enc.position    = [2, 4, 6];
  enc.orientation = [0, 2, 4, 6];
  enc.num_ranges  = 2;
  enc.ranges      = [7, 6];
  enc.name        = 'foobar string';
  enc.enabled     = false;
  enc.nExamples1  = 0;
  enc.nExamples2  = 0;
  enc.subExamples = [];
  enc.subStrings  = [];

  z.subscribe(chan, zcmtypes.encoded_t, function(channel, msg){
    const recEnc = zcmtypes.example_t.decode(msg.msg);
    console.log("Encoded message received on channel " + channel + ": " + recEnc.name);
  }, function successCb(_sub) { encSub = _sub; });

  setInterval(function() {
    var msg = new zcmtypes.encoded_t();
    const buf = enc.encode();
    msg.n = buf.length;
    msg.msg = buf;
    z.publish(chan, msg);
  }, 1000);
}

function bitfieldExample()
{
  assert(zcmtypes.bitfield_t.FIELD22_TEST == 255);
  assert(zcmtypes.bitfield_t.FIELD23_TEST ==   3);
  assert(zcmtypes.bitfield_t.FIELD24_TEST ==   7);

  assert(zcmtypes.bitfield_t.SIGN_TEST_0  == 0x0f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_1  ==  -16);
  assert(zcmtypes.bitfield_t.SIGN_TEST_2  == 0x7f);
  assert(zcmtypes.bitfield_t.SIGN_TEST_3  == -128);

  assert(zcmtypes.bitfield_t.SIGN_TEST_4  == 0x1fff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_5  ==  -8192);
  assert(zcmtypes.bitfield_t.SIGN_TEST_6  == 0x7fff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_7  == -32768);

  assert(zcmtypes.bitfield_t.SIGN_TEST_8  ==  0x01ffffff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_9  ==   -33554432);
  assert(zcmtypes.bitfield_t.SIGN_TEST_10 ==  0x7fffffff);
  assert(zcmtypes.bitfield_t.SIGN_TEST_11 == -2147483648);

  assert(zcmtypes.bitfield_t.SIGN_TEST_12 == "-1");
  assert(zcmtypes.bitfield_t.SIGN_TEST_13 == "72057594037927935");
  assert(zcmtypes.bitfield_t.SIGN_TEST_14 == "-72057594037927936");
  assert(zcmtypes.bitfield_t.SIGN_TEST_15 == "9223372036854775807");
  assert(zcmtypes.bitfield_t.SIGN_TEST_16 == "-9223372036854775808");

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
  assert(zcmtypes.bitfield_t.SIGN_TEST_27 == "9223372036854775807");

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
  assert(zcmtypes.bitfield_t.SIGN_TEST_42 == "9223372036854775807");
  assert(zcmtypes.bitfield_t.SIGN_TEST_43 == -1);
  assert(zcmtypes.bitfield_t.SIGN_TEST_44 == "9223372036854775807");
  assert(zcmtypes.bitfield_t.SIGN_TEST_45 == -1);

  setInterval(function() {
    var b = new zcmtypes.bitfield_t();
    b.field1 = 3;
    b.field2 = [ [1, 0, 1, 0], [1, 0, 1, 0] ];
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
    b.field18 =        -1;
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
    b.field33 = bigint("7fffffffffffffff", 16);
    b.field34 = bigint("7fffffffffffffff", 16);
    z.publish("BITFIELD", b);
  }, 1000);
}

basicExample();
recursiveExample();
multidimExample();
packageExample();
encodeExample();
bitfieldExample();

// Intentionally not saving the subscription here to make sure we don't
// segfault due to not tracking the subscription in user-space
z.subscribe("RECURSIVE_EXAMPLE", zcmtypes.recursive_t, function(channel, msg) {
  console.log("Typed message received on channel " + channel);
  assert('e' in msg && 'timestamp' in msg.e &&
         msg.e.timestamp == zcmtypes.example_t.test_const_32_max_hex, "Wrong msg received");
}, function successCb (_sub) {
});

var typedSub2;
z.subscribe("PACKAGED", zcmtypes.test_package.packaged_t, function(channel, msg) {
  console.log("Typed message received on channel " + channel);
}, function successCb (_sub) {
  typedSub2 = _sub;
});

z.subscribe("BITFIELD", zcmtypes.bitfield_t, (channel, msg) => {
  console.log("Message received on channel", channel);
  if (msg.field1 != -1) console.error("Failed to decode field1");
  if (msg.field2[0][0] != -1) console.error("Failed to decode field2");
  if (msg.field2[0][1] !=  0) console.error("Failed to decode field2");
  if (msg.field2[0][2] != -1) console.error("Failed to decode field2");
  if (msg.field2[0][3] !=  0) console.error("Failed to decode field2");
  if (msg.field2[1][0] != -1) console.error("Failed to decode field2");
  if (msg.field2[1][1] !=  0) console.error("Failed to decode field2");
  if (msg.field2[1][2] != -1) console.error("Failed to decode field2");
  if (msg.field2[1][3] !=  0) console.error("Failed to decode field2");
  if (msg.field3 != -1) console.error("Failed to decode field3");
  if (msg.field4 != -3) console.error("Failed to decode field4");
  if (msg.field5 != 7) console.error("Failed to decode field5");
  if (msg.field6 != 0) console.error("Failed to decode field6");
  if (msg.field7 != 0) console.error("Failed to decode field7");
  if (msg.field8_dim1 != 0) console.error("Failed to decode field8_dim1");
  if (msg.field8_dim2 != 0) console.error("Failed to decode field8_dim2");
  if (msg.field8.length != 0) console.error("Failed to decode field8");
  if (msg.field9 != ~(1 << 27) + 1) console.error("Failed to decode field9");
  if (msg.field10.toString() != bigint(1).shiftLeft(52).or(1).toString()) {
    console.error("Failed to properly decode bitfield");
  }
  if (msg.field11 != 3) console.error("Failed to decode field11");
  for (let i = 0; i < 3; ++i) {
    for (let j = 0; j < 2; ++j) {
      for (let k = 0; k < 2; ++k) {
        for (let l = 0; l < 2; ++l) {
          if (msg.field12[i][j][k][l] != k + l + 1) {
            console.error(`Failed to decode field12[${i}][${j}][${k}][${l}]`);
            console.error(`${msg.field12[i][j][k][l]} != ${k + l + 1}`);
          }
        }
      }
    }
  }
  if (msg.field15 != -60) console.error("Failed to decode field15");
  if (msg.field16 != 2) console.error("Failed to decode field16");
  if (msg.field18 != 15) console.error("Failed to decode field18");
  if (msg.field19 != 68) console.error("Failed to decode field19");
  if (msg.field20 != 2) console.error("Failed to decode field20");
  if (msg.field22 != msg.FIELD22_TEST) "Bad decode of field 22"
  if (msg.field23 != msg.FIELD23_TEST) "Bad decode of field 23"
  if (msg.field24 != msg.FIELD24_TEST) "Bad decode of field 24"
  if (msg.field25 != 3) console.log("Bad decode of field 25");
  if (msg.field26 != 255) console.log("Bad decode of field 26");
  if (msg.field27 != 3) console.log("Bad decode of field 27");
  if (msg.field28 != 0x7f) console.log("Bad decode of field 28");
  if (msg.field29 != 3) console.log("Bad decode of field 29");
  if (msg.field30 != 0x7fff) console.log("Bad decode of field 30");
  if (msg.field31 != 0xf) console.log("Bad decode of field 31");
  if (msg.field32 != 0x7fffffff) console.log("Bad decode of field 32");
  if (msg.field33 != 0xf) console.log("Bad decode of field 33");
  if (msg.field34 != 0x7fffffffffffffff) console.log("Bad decode of field 34");
}, () => {});

var sub;
z.subscribe(".*", null, function(channel, msg) {
  console.log("Untyped message received on channel " + channel);
}, function successCb (_sub) {
  sub = _sub;
});

process.on('exit', function() {
  if (sub) z.unsubscribe(sub);
  if (typedSub2) z.unsubscribe(typedSub2);
  z.destroy();
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});
