#!/bin/bash

set -e

pushd /
git clone https://github.com/facebook/buck.git
cd buck
ANT_OPTS=-Xmx1024m ant default
popd
