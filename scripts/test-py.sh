#!/bin/bash

THISDIR=$(pwd)

## Note: this runs in a subshell
gen_lcm() {(
    file=$1
    rm -fr /tmp/lcmtypes
    mkdir /tmp/lcmtypes
    cd /tmp/lcmtypes
    lcm-gen -p $file
    sed -i 's/lcm/zcm/g' /tmp/lcmtypes/*
    sed -i 's/LCM/ZCM/g' /tmp/lcmtypes/*
)}

## Note: this runs in a subshell
gen_zcm() {(
    file=$1
    rm -fr /tmp/zcmtypes
    mkdir /tmp/zcmtypes
    cd /tmp/zcmtypes
    $THISDIR/build/gen/zcm-gen -p $file
)}

ZCMFILES=$(ls test/zcm-gen/*.zcm)

for f in $ZCMFILES; do
    name=$(basename $f)
    bname=${name%.*}
    gen_lcm $THISDIR/$f
    gen_zcm $THISDIR/$f
    diff /tmp/lcmtypes/$bname.py /tmp/zcmtypes/$bname.py
done
