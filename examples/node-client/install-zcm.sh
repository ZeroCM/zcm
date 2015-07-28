#!/bin/bash
if [ "$1" == "local" ]; then
    cp ../../zcm/js/node/index.js node_modules/zcm/.
else
    npm install ../../build/zcm/js/zcm-*.tgz
fi

cp ../../build/zcm/js/zcm-client.js public/js/.
