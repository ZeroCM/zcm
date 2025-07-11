const zcm = require('zerocm');
const zcmtypes = require('zcmtypes');
const z = zcm.create(zcmtypes, 'inproc');
if (!z) throw 'Failed to create ZCM';

const tests = [require('./bitfield-test'), require('./example-test')].map(t => t(z, zcmtypes));

Promise.all(tests)
  .then(() => {
    console.log('All tests passed!');
    z.destroy();
    process.exit(0);
  })
  .catch(err => {
    console.error(err);
    z.destroy();
    process.exit(1);
  });
