#!/bin/bash -ev

mkdir -p ~/.axe
echo "rpcuser=username" >>~/.axe/axe.conf
echo "rpcpassword=`head -c 32 /dev/urandom | base64`" >>~/.axe/axe.conf
