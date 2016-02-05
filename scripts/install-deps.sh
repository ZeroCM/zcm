#!/bin/bash

PKGS=''

## Lib ZMQ
PKGS+='libzmq3 libzmq3-dev '

## Java
PKGS+='default-jdk default-jre '

## Node
PKGS+='nodejs nodejs-legacy npm '

sudo apt-get update
sudo apt-get install -yq $PKGS
