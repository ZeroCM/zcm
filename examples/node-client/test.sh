#!/bin/bash
set -euo pipefail
cd ../../
rm -f build/zcm/js/zerocm-1.2.0.tgz && ./waf build -d && sudo ./waf install -d
cd -
rm -rf node_modules
npm i
cd node_modules/zerocm
V=1 node-gyp clean configure build --debug
cd -
# NODE_ENV=dev gdb --tui --args node app.js
