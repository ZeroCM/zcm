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
USE_JULIA=true
JULIA_0_6_MODE=false
USE_NODE=true
while getopts "ijms" opt; do
    case $opt in
        i) STRICT=false ;;
        j) USE_JULIA=false ;;
        m) JULIA_0_6_MODE=true ;;
        n) USE_NODE=false ;;
        s) SINGLE_MODE=true ;;
        \?) exit 1 ;;
    esac
done

mkdir -p $ROOTDIR/deps

PKGS=''
PIP_PKGS=''

## Dependency dependencies
PKGS+='mlocate wget '

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

sudo pip install $PIP_PKGS
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to install pip packages"
    exit $ret
fi

## Node
if $USE_NODE; then
    bash -c "export NVM_DIR=\$HOME/.nvm; [ -s \"\$NVM_DIR/nvm.sh\" ] && \\. \"\$NVM_DIR/nvm.sh\"; nvm --version" > /dev/null 2>&1
    nvmExists=$?
    if [ $nvmExists -ne 0 ]; then
        echo "Downloading NVM"
        unset NVM_DIR
        outfile=$(mktemp)
        wget -q -O$outfile https://raw.githubusercontent.com/creationix/nvm/v0.33.11/install.sh
        echo "Installing NVM"
        chmod +x $outfile
        $outfile
        rm $outfile
    fi
    echo "Installing node version 4.2.6"
    bash -c "export NVM_DIR=\$HOME/.nvm; [ -s \"\$NVM_DIR/nvm.sh\" ] && \\. \"\$NVM_DIR/nvm.sh\"; nvm install 4.2.6 && nvm alias default 4.2.6"
fi

## Julia
checkJuliaInstall()
{
    juliaVersion=$(julia --version 2>/dev/null)
    juliaExists=$?
    juliaVersion=$(echo "$juliaVersion" | xargs | cut -d ' ' -f 3)

    expectedVersion="1.3.1"
    if $JULIA_0_6_MODE; then
        expectedVersion="0.6.4"
    fi

    if [ $juliaExists -ne 0 ] || [ "$juliaVersion" != "$expectedVersion" ]; then
        return 1
    else
        return 0
    fi
}
if $USE_JULIA; then
    checkJuliaInstall
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "Installing julia"
        tmpdir=$(mktemp -d)
        pushd $tmpdir > /dev/null

        if $JULIA_0_6_MODE; then
            wget https://julialang-s3.julialang.org/bin/linux/x64/0.6/julia-0.6.4-linux-x86_64.tar.gz
            tar -xaf julia-0.6.4-linux-x86_64.tar.gz
            rm -rf $ROOTDIR/deps/julia
            mv julia-9d11f62bcb $ROOTDIR/deps/julia
        else
            wget https://julialang-s3.julialang.org/bin/linux/x64/1.3/julia-1.3.1-linux-x86_64.tar.gz
            tar -xaf julia-1.3.1-linux-x86_64.tar.gz
            rm -rf $ROOTDIR/deps/julia
            mv julia-1.3.1 $ROOTDIR/deps/julia
        fi

        popd > /dev/null
        rm -rf $tmpdir
        echo "Julia has been downloaded to $ROOTDIR/deps"
        echo -n "$color_bold$color_redf"
        echo    "You must add the following to your ~/.bashrc:"
        echo    "    PATH=\$PATH:$ROOTDIR/deps/julia/bin"
        echo -n "$color_reset"
    else
        echo "Found julia on system. Skipping install"
    fi
fi

echo "Updating db"
sudo updatedb
ret=$?
if [[ $ret -ne 0 && "$STRICT" == "true" ]]; then
    echo "Failed to updatedb"
    exit $ret
fi

exit 0
