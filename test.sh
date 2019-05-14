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

echo "This script is currently non-functional. If zcm built, it's probably working"

## Test type generation
# run   zcm-gen-test    ./scripts/test-gen.sh

## Test subscribe and unsubscribe
run   coretest        ./build/build_examples/examples/test/zcm/coretest
run   sub-unsub-c     ./build/build_examples/examples/test/zcm/sub_unsub_c
run   sub-unsub-cpp   ./build/build_examples/examples/test/zcm/sub_unsub_cpp
run   api-retcodes    ./build/build_examples/examples/test/zcm/api_retcodes
run   dispatch-loop   ./build/build_examples/examples/test/zcm/dispatch_loop
run   forking         ./build/build_examples/examples/test/zcm/forking
run   forking2        ./build/build_examples/examples/test/zcm/forking2
run   flushing        ./build/build_examples/examples/test/zcm/flushing
run   logging         ./build/build_examples/examples/test/zcm/logtest
run   trackers        ./build/build_examples/examples/test/zcm/trackers
