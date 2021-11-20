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

build_examples() {
    set +u +o pipefail
    trap - ERR
    source $ROOTDIR/examples/env $1
    set -u -o pipefail
    trap command_failed ERR
    $ROOTDIR/waf build_examples$2
    $ROOTDIR/waf build_tests$2
    $ROOTDIR/test/run-tests.sh "$2"
}

# Basic build
$ROOTDIR/waf distclean configure build

# Full build
$ROOTDIR/waf distclean
$ROOTDIR/waf configure --use-all --use-third-party --use-dev \
                       --track-traffic-topology=true --prefix=$tmpdir
$ROOTDIR/waf build
$ROOTDIR/waf install
(build_examples $tmpdir "")

# Full build asan
$ROOTDIR/waf distclean
$ROOTDIR/waf configure --use-all --use-third-party --use-dev \
                       --track-traffic-topology=true --prefix=$tmpdirAsan
$ROOTDIR/waf build_asan
$ROOTDIR/waf install_asan
(build_examples $tmpdirAsan "_asan")
