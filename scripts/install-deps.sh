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
ROOTDIR=${THISDIR%/scripts}
####

color_bold=`tput bold`
color_redf=`tput setaf 1`
color_reset=`tput sgr0`

STRICT=true
SINGLE_MODE=false
while getopts "is" opt; do
    case $opt in
        i) STRICT=false ;;
        s) SINGLE_MODE=true ;;
        \?) exit 1 ;;
    esac
done

mkdir -p $ROOTDIR/deps

PKGS=''
PIP_PKGS=''

## Dependency dependencies
PKGS+='wget '

## Waf dependencies
PKGS+='pkg-config '

## Basic C compiler dependency
PKGS+='build-essential '

## Lib ZMQ
PKGS+='libzmq3-dev '

## Java
PKGS+='default-jdk default-jre '

## Python
PKGS+='python python-pip '
PIP_PKGS+='Cython '

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
if $SINGLE_MODE; then
    for pkg in $PKGS; do
        echo "Installing $pkg"
        sudo apt-get install -yq $pkg
        ret=$?
        if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
            echo "Failed to install packages"
            exit $ret
        fi
    done
else
    sudo apt-get install -yq $PKGS
fi

sudo pip install --upgrade pip
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to upgrade pip"
    exit $ret
fi

pip install --user $PIP_PKGS
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to install pip packages"
    exit $ret
fi

## Node
bash -i -c "which node" > /dev/null 2>&1
nodeExists=$?
if [ $nodeExists -ne 0 ]; then
    echo "Installing NVM"
    unset NVM_DIR
    NVM_INSTALL=$(wget -qO- https://raw.githubusercontent.com/creationix/nvm/v0.33.11/install.sh)
    echo "$NVM_INSTALL" | bash -i
    bash -i -c "nvm install 4.2.6 && nvm alias default 4.2.6"
else
    echo "Found node on system. Skipping install"
fi

## Julia
checkJuliaInstall()
{
    juliaVersion=$(julia --version 2>/dev/null)
    juliaExists=$?
    juliaVersion=$(echo "$juliaVersion" | xargs | cut -d ' ' -f 3)
    if [ $juliaExists -ne 0 ] || [ "$juliaVersion" != "0.6.4" ]; then
        return 1
    else
        return 0
    fi
}
checkJuliaInstall
ret=$?
if [ $ret -ne 0 ]; then
    echo "Installing julia"
    tmpdir=$(mktemp -d)
    pushd $tmpdir > /dev/null
    wget https://julialang-s3.julialang.org/bin/linux/x64/0.6/julia-0.6.4-linux-x86_64.tar.gz
    tar -xaf julia-0.6.4-linux-x86_64.tar.gz
    mv julia-9d11f62bcb $ROOTDIR/deps/
    popd > /dev/null
    rm -rf $tmpdir
    echo "Julia has been downloaded to $ROOTDIR/deps"
    echo -n "$color_bold$color_redf"
    echo    "You must add the following to your ~/.bashrc:"
    echo    "    PATH=\$PATH:$ROOTDIR/deps/julia-9d11f62bcb/bin"
    echo -n "$color_reset"
else
    echo "Found julia on system. Skipping install"
fi

echo "Updating db"
sudo updatedb
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to updatedb"
    exit $ret
fi

exit 0
