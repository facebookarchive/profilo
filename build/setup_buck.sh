#!/bin/bash

set -e

pushd /
git clone --depth 500 https://github.com/facebook/buck.git
cd buck
# check out known good buck version from Jun 29 07:46:12 2020:
git checkout 3283f1ca2bb3ce3aa809da5f516c42009ec3a1d9
ANT_OPTS=-Xmx1024m ant default
popd
