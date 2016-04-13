#!/bin/bash

PKGS=''

## Basic C compiler dependency
PKGS+='build-essential '

## Lib ZMQ
PKGS+='libzmq3-dev '

## Java
PKGS+='default-jdk default-jre '

## Node
PKGS+='nodejs nodejs-legacy npm '

## CxxTest
PKGS+='cxxtest '

sudo apt-get update
sudo apt-get install -yq $PKGS
