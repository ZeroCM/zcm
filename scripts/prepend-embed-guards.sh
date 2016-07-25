#!/bin/bash

for file in `find $1 -type f`; do
  sed -i '1i #define ZCM_EMBEDDED' "$file"
  sed -i '1i #undef ZCM_EMBEDDED' "$file"
  sed -i '1i \ *\/' "$file"
  sed -i '1i \ * DO NOT MODIFY THIS FILE BY HAND' "$file"
  sed -i '1i \/*' "$file"
done
