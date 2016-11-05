#!/bin/bash
if [ "$1" == "local" ]; then
    mkdir -p node_modules/zcm
    cp ../../zcm/js/node/index.js node_modules/zcm/
#else
    #npm install ../../build/zcm/js/zcm-*.tgz
fi

cp ../../build/zcm/js/zcm-client.js public/js/.
