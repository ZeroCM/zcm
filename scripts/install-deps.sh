#!/bin/bash

STRICT=true
while getopts "i" opt; do
    case $opt in
        i) STRICT=false ;;
        \?) exit 1 ;;
    esac
done

PKGS=''

## Waf dependencies
PKGS+='pkg-config '

## Basic C compiler dependency
PKGS+='build-essential '

## Lib ZMQ
PKGS+='libzmq3-dev '

## Java
PKGS+='default-jdk default-jre '

## Node
PKGS+='nodejs nodejs-legacy npm '

## Python
PKGS+='cython '

## LibElf
PKGS+='libelf-dev libelf1 '

## CxxTest
PKGS+='cxxtest '

## Clang tools for code sanitizers, style checkers, etc.
PKGS+='clang '

echo "Updating apt repos"
sudo apt-get update
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to update"
    exit $ret
fi
echo "Installing from apt"
for pkg in $PKGS; do
    echo "Installing $pkg"
    sudo apt-get install -yq $pkg
    ret=$?
    if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
        echo "Failed to install packages"
        exit $ret
    fi
done
echo "Updating db"
sudo updatedb
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to updatedb"
    exit $ret
fi

exit 0
