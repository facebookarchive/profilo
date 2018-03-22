#!/bin/bash

## THIS IS SUPPOSED TO RUN INSIDE THE CONTAINER

set -euxo pipefail
ln -s buck_imports/buckconfig .buckconfig
buck fetch deps/...
buck test //java/test/com/facebook/profilo/logger:logger
