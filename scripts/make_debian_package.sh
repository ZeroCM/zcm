#!/bin/bash

## SETUP

# In this directory we assemble the deb package. Later we call dpkg-deb on it to pack the package.
DEB_PACKAGE_ASSEMBLY_DIR=./build/deb_package_root
mkdir -p $DEB_PACKAGE_ASSEMBLY_DIR/usr/

# Required to find java
export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64/

# Change to the directory containing the source code
THISDIR=$(dirname "$(readlink -f "$0")")
BASEDIR=$(dirname $THISDIR)
cd $BASEDIR


## BUILD

# Build with python2 support and install to temporary $DEB_PACKAGE_ASSEMBLY_DIR/usr directory
export PYTHON=/usr/bin/python2
./waf configure --use-all --use-third-party --use-clang -d -s --prefix=$DEB_PACKAGE_ASSEMBLY_DIR/usr/
./waf build
./waf install

# Build again for python3 and install to the temporary $DEB_PACKAGE_ASSEMBLY_DIR/usr directory.
# Note 1: This overrides most of the already existing files except for the python2
#         files in usr/lib/python2.7 <- this is to be considered an ugly hack but
#         I found no other way to make waf build for python2 AND python3
# Note 2: we use --targets=pyzcm to hopefully not build everything again
export PYTHON=/usr/bin/python3
./waf configure --use-all --use-third-party --use-clang -d -s --prefix=$DEB_PACKAGE_ASSEMBLY_DIR/usr/
./waf build --targets=pyzcm
./waf install


### HACKS TO PREPARE DEBIAN PACKAGE STRUCTURE

# Move the debian control files directory to the temporary $DEB_PACKAGE_ASSEMBLY_DIR
cp -r ./build/DEBIAN $DEB_PACKAGE_ASSEMBLY_DIR


cd $DEB_PACKAGE_ASSEMBLY_DIR
# Unfortunately waf automatically installs to 'pythonX.X/site-packages' as soon as the
# root directory is not contained in the install prefix.
# We need it in 'dist-packages' so we just rename it manually here.
# Note: since this modifies the folder structure that 'find' is iterating, it causes
#       find to print an error such as:
#       "find: ‘./usr/lib/python3.6/site-packages’: No such file or directory".
# It works anyways ...
find -type d -wholename '*python*/site-packages' -execdir mv ./site-packages ./dist-packages \;

# There are a number of files in which the install prefix appears such as the java
# launchers in usr/bin and the package-config files.
# This is undesirable since the temporary install prefix in $DEB_PACKAGE_ASSEMBLY_DIR
# is obviously wrong after the files have been installed.
# The following lines replaces all occurences of the $DEB_PACKAGE_ASSEMBLY_DIR as
# path with '/usr' which is our actual install prefix with the debian package.
find -type f -exec sed -i "s+$PWD++g" {} +
cd -

### PACK DEBIAN PACKAGE
##  Debian compliance: fakeroot is required to get correct uids and gids for all installed files
fakeroot dpkg-deb -b $DEB_PACKAGE_ASSEMBLY_DIR
dpkg-name $DEB_PACKAGE_ASSEMBLY_DIR.deb
