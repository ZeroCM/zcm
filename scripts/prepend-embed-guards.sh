#!/bin/bash

dir=${1%%/}

files=`find $1 -type f`
ret=$?

if [ $ret -ne 0 ]; then
    exit $ret
fi

for file in $files; do
    sed -i .old '1s;^;#define ZCM_EMBEDDED;' "$file"
    sed -i .old '1s;^;#undef ZCM_EMBEDDED;' "$file"
    sed -i .old '1s;^;\ *\/;' "$file"
    sed -i .old '1s;^;\ * DO NOT MODIFY THIS FILE BY HAND;' "$file"
    sed -i .old '1s;^;\/*;' "$file"
done
