#!/bin/bash

THISDIR=$(pwd)
TESTDIR=test/zcm-gen
TMPDIR=/tmp/zcmtypes

VERBOSE=0
if [ "$1" == "-v" ]; then
    VERBOSE=1
fi

## Note: this runs in a subshell
gen_zcm() {(
    file=$1
    rm -fr $TMPDIR
    mkdir $TMPDIR
    cd $TMPDIR
    $THISDIR/build/gen/zcm-gen -c $file
)}

check() {
    bname=$1
    ext=$2
    fname=$bname.$ext

    cand=$TMPDIR/$fname
    orig=$TESTDIR/$bname.ans/$fname

    diff $cand $orig > /tmp/zcm-test.out 2>&1
    if [ "$?" != "0" ]; then
        echo "Error: mismatch btwn $cand and $orig"
        if [ "$VERBOSE" == "1" ]; then
            cat /tmp/zcm-test.out
        fi
    fi
}

compare() {
    bname=$1
    check $bname h
    check $bname c
}

ZCMFILES=$(ls $TESTDIR/*.zcm)

for f in $ZCMFILES; do
    name=$(basename $f)
    bname=${name%.*}
    gen_zcm $THISDIR/$f
    compare $bname
    # diff /tmp/lcmtypes/$bname.h /tmp/zcmtypes/$bname.h
    # diff /tmp/lcmtypes/$bname.c /tmp/zcmtypes/$bname.c
done
