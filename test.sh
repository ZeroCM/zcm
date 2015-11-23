#!/bin/sh

run()
{
    name=$1
    shift
    echo "\033[1m== Running: $name ==\033[0m"
    $@
    if [ "$?" != "0" ]; then
        echo "FAILED"
    fi
}

## Test type generation
run   zcm-gen-test    ./scripts/test-gen.sh

## Test subscribe and unsubscribe
run   sub-unsub-c     ./build/test/zcm/sub_unsub_c
run   sub-unsub-cpp   ./build/test/zcm/sub_unsub_cpp
