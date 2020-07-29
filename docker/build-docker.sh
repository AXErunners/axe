#!/usr/bin/env bash

export LC_ALL=C

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

DOCKER_IMAGE=${DOCKER_IMAGE:-axerunners/axed-development}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/axed docker/bin/
cp $BUILD_DIR/src/axe-cli docker/bin/
cp $BUILD_DIR/src/axe-tx docker/bin/
strip docker/bin/axed
strip docker/bin/axe-cli
strip docker/bin/axe-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
