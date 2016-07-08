#!/bin/bash

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

## CxxTest
PKGS+='cxxtest '

sudo apt-get update
sudo apt-get install -yq $PKGS
