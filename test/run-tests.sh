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

BLD=tests
if [ $# -ne 0 ]; then
    BLD=$BLD$1
fi

#### We want to exit with an error code if anything goes wrong
set -o errtrace
set -uo pipefail
function command_failed() {
    errcode=$?
    echo -e "\nERROR: Command on line ${BASH_LINENO[0]} failed ($errcode): $BASH_COMMAND\n"
    exit $errcode
}
trap command_failed ERR

export LD_LIBRARY_PATH=$ROOTDIR/build/$BLD/test/types:$LD_LIBRARY_PATH

export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::")
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"

echo
echo
echo "**********************************"
echo "Running core tests"
echo "**********************************"
$ROOTDIR/build/$BLD/test/runner
echo "Success"

if [ $# -ne 0 ]; then
    echo "Skipping non c/c++ lanugage tests in sanitizer mode"
    exit 0
fi

echo
echo
echo "**********************************"
echo "Running python tests"
echo "**********************************"
$PYTHON $THISDIR/python/bitfield-test.py
$PYTHON $THISDIR/python/example-test.py
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
