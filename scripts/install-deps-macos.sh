#!/bin/bash

STRICT=true
SINGLE_MODE=false
while getopts "is" opt; do
    case $opt in
        i) STRICT=false ;;
        s) SINGLE_MODE=true ;;
        \?) exit 1 ;;
    esac
done


## -----------------------------------------------------------------------------
## Define the dependencies to be installed
PKGS=''
PIP_PKGS=''

## Waf dependencies
PKGS+='pkg-config '

## ZMQ
PKGS+='zeromq '

## Java
PKGS+='caskroom/cask/java '

## Node
PKGS+='nodejs npm '

## Python
PKGS+='python '
PIP_PKGS+='Cython '

## LibElf
PKGS+='libelf '

## CxxTest
PKGS+='cxxtest '


## -----------------------------------------------------------------------------
## Install Xcode command line tools
xcode-select --install


## -----------------------------------------------------------------------------
## Install Brew dependencies
echo "Installing from brew"
if $SINGLE_MODE; then
    for pkg in $PKGS; do
        echo "Installing $pkg"
        brew install $pkg
        ret=$?
        if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
            echo "Failed to install packages"
            exit $ret
        fi
    done
else
    brew install $PKGS
fi

sudo ln -s /usr/local/Cellar/libelf/0.8.13_1/include/libelf/libelf.h /usr/include/libelf.h
sudo ln -s /usr/local/Cellar/libelf/0.8.13_1/include/libelf/gelf.h /usr/include/gelf.h


## -----------------------------------------------------------------------------
## Install PIP dependencies
sudo pip install --upgrade pip
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to upgrade pip"
    exit $ret
fi

sudo pip install $PIP_PKGS
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to install pip packages"
    exit $ret
fi


exit 0
