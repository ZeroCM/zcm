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

echo
echo
echo "**********************************"
echo "Running core tests"
echo "**********************************"
$ROOTDIR/build/tests/test/runner
echo "Success"

echo
echo
echo "**********************************"
echo "Running python tests"
echo "**********************************"
$PYTHON $THISDIR/python/bitfield-test.py
echo "Success"

echo
echo
echo "**********************************"
echo "Running node tests"
echo "**********************************"
pushd $THISDIR/node
nvm use
rm -rf node_modules
npm i --unsafe-perm
node index.js
echo "Success"
