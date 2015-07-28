#!/bin/bash

PKGS=''

## Lib ZMQ
PKGS+='libzmq3 libzmq3-dev python-zmq '

sudo apt-get update
sudo apt-get install -yq $PKGS
