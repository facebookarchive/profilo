#!/bin/bash

set -e

pushd /
git clone https://github.com/facebook/buck.git
cd buck
ANT_OPTS=-Xmx512m ant default
popd
