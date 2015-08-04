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
    $THISDIR/build/gen/zcm-gen -c -x -j -p $file >/dev/null
)}

check_all_ext() {
    bname=$1
    ext=$2

    ## find all files with the requested extention in both this dir and
    candfiles=$(find $TMPDIR | grep "\.$ext"'$')
    origfiles=$(find $TESTDIR/$bname.ans | grep "\.$ext"'$')
    collected=($candfiles $origfiles)
    files=$(echo "${collected[@]}" | xargs -n 1 basename | sort -u)

    for f in $files; do
        if [ "$ext" == "java" ]; then
            cand=$TMPDIR/zcmtypes/$f
            orig=$TESTDIR/$bname.ans/zcmtypes/$f
        else
            cand=$TMPDIR/$f
            orig=$TESTDIR/$bname.ans/$f
        fi
        if [ "$VERBOSE" == "1" ]; then
            echo "CHECK: '$cand' VS '$orig'"
        fi
        diff $cand $orig > /tmp/zcm-test.out 2>&1
        if [ "$?" != "0" ]; then
            echo "Error: mismatch btwn $cand and $orig"
            if [ "$VERBOSE" == "1" ]; then
                cat /tmp/zcm-test.out
            fi
        fi
    done
}

compare_c() {
    bname=$1
    check_all_ext $bname h
    check_all_ext $bname c
}

compare_cpp() {
    bname=$1
    check_all_ext $bname hpp
}

compare_java() {
    bname=$1
    check_all_ext $bname java
}

compare_python() {
    bname=$1
    check_all_ext $bname py
}

## Add new language entries here
compare() {
    bname=$1
    compare_c $bname
    compare_cpp $bname
    compare_python $bname
    compare_java $bname
}

ZCMFILES=$(ls $TESTDIR/*.zcm)

for f in $ZCMFILES; do
    name=$(basename $f)
    bname=${name%.*}

    if [ "$VERBOSE" == "1" ]; then
        echo "TEST: $bname"
    fi

    gen_zcm $THISDIR/$f
    compare $bname
done
