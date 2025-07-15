#!/bin/bash
set -euo pipefail
rm -rf node_modules
npm i
cd node_modules/zerocm
V=1 node-gyp clean configure build --debug
cd -
# NODE_ENV=dev gdb --tui --args node app.js
