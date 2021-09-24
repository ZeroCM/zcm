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

export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::")
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"

tmpdir=$(mktemp -d)
tmpdirAsan=$(mktemp -d)

build_examples() {
    source $ROOTDIR/examples/env $1
    $ROOTDIR/waf build_examples
    $ROOTDIR/waf build_tests
    $ROOTDIR/build/tests/test/runner
}

# Basic build
$ROOTDIR/waf distclean configure build

# Full build
$ROOTDIR/waf distclean
$ROOTDIR/waf configure --use-all --use-third-party --use-dev \
                       --track-traffic-topology=true --prefix=$tmpdir
$ROOTDIR/waf build
$ROOTDIR/waf install
(build_examples $tmpdir)

# Full build asan
$ROOTDIR/waf distclean
$ROOTDIR/waf configure --use-all --use-third-party --use-dev \
                       --track-traffic-topology=true --prefix=$tmpdirAsan
$ROOTDIR/waf build_asan
$ROOTDIR/waf install_asan
(build_examples $tmpdir64)
