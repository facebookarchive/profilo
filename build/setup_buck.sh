#!/bin/bash

set -e

pushd /
git clone https://github.com/facebook/buck.git
cd buck
ant default
popd
