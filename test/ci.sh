#!/bin/bash

#### Find the script directory regardless of symlinks, etc
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
    DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
    # if $SOURCE was a relative symlink, we need to resolve it relative
    # to the path where the symlink file was located
done
THISDIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
ROOTDIR=${THISDIR%/test}
####

#### We want to exit with an error code if anything goes wrong
set -o errtrace
set -uo pipefail
function command_failed() {
    errcode=$?
    echo -e "\nERROR: Command on line ${BASH_LINENO[0]} failed ($errcode): $BASH_COMMAND\n"
    exit $errcode
}
trap command_failed ERR

export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::")
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"

tmpdir=$(mktemp -d)
tmpdirAsan=$(mktemp -d)

cd $ROOTDIR

build_examples() {
    set +u +o pipefail
    trap - ERR
    source ./examples/env $1
    set -u -o pipefail
    trap command_failed ERR
    ./waf build_examples$2
    ./waf build_tests$2
    if [ -n "$2" ]; then ln -sf $ROOTDIR/build/tests"$2" ./build/tests; fi
    ./test/run-tests.sh "$2"
}

# Basic build
./waf distclean configure build

# Full build
./waf distclean
./waf configure --use-all --use-third-party --use-dev \
                --track-traffic-topology=true --prefix=$tmpdir
./waf build
./waf install
(build_examples $tmpdir "")

# Full build asan
./waf distclean
./waf configure --use-all --use-third-party --use-dev \
                --track-traffic-topology=true --prefix=$tmpdirAsan
./waf build_asan
./waf install_asan
(build_examples $tmpdirAsan "_asan")
