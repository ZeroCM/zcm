const zcm = require('zerocm');
const zcmtypes = require('zcmtypes');
const z = zcm.create(zcmtypes, 'inproc');
if (!z) throw 'Failed to create ZCM';

const tests = [require('./bitfield-test'), require('./example-test')].map(t => t(z, zcmtypes));

Promise.all(tests).catch(err => {
  console.error(err);
  process.exit(1);
});
