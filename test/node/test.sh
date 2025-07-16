#!/bin/bash
. ~/.nvm/nvm.sh
nvm use 16
npm i -g node-gyp
set -euo pipefail
cd ~/Private/gcs/external/commlib2/external/zcm
rm -f build/zcm/js/zerocm-1.2.0.tgz && ./waf build -d && ./waf install -d
cd test/node
rm -rf node_modules
npm i
pushd node_modules/zerocm
V=1 node-gyp clean configure build --debug
popd
if [[ -n "${USE_GDB:-}" ]]; then
    NODE_ENV=dev gdb --tui --args node index.js
else
    NODE_ENV=dev node index.js
fi
