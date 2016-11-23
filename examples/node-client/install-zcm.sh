#!/bin/bash
if [ "$1" == "local" ]; then
    mkdir -p node_modules/zcm
    cp ../../zcm/js/node/index.js node_modules/zcm/
# RRR: is this no longer necessary? should we delete it or replace it
#else
    #npm install ../../build/zcm/js/zcm-*.tgz
fi

cp ../../build/zcm/js/zcm-client.js public/js/.
