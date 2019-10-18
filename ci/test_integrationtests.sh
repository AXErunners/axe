#!/usr/bin/env bash

# This script is executed inside the builder image

set -e

PASS_ARGS="$@"

source ./ci/matrix.sh

if [ "$RUN_INTEGRATIONTESTS" != "true" ]; then
  echo "Skipping integration tests"
  exit 0
fi

export LD_LIBRARY_PATH=$BUILD_DIR/depends/$HOST/lib

cd build-ci/axecore-$BUILD_TARGET

#./test/functional/test_runner.py --coverage $PASS_ARGS
