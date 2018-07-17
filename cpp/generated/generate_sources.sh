#!/bin/bash

set -e
set -o pipefail
REPO_ROOT=$(hg root)
OUTPUT_PATH="$REPO_ROOT/fbandroid/libraries/profilo/cpp/generated"
# TODO(T31664478): Replace this target with an alias
CODEGEN_TARGET="//libraries/profilo/cpp/codegen:codegen"

buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_types > $OUTPUT_PATH/EntryType.h
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_types_cpp > $OUTPUT_PATH/EntryType.cpp
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_structs > $OUTPUT_PATH/Entry.h
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_structs_cpp > $OUTPUT_PATH/Entry.cpp
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode parser > $OUTPUT_PATH/EntryParser.h

buck run ${CODEGEN_TARGET} -- --lang java --module android --mode entry_types > $OUTPUT_PATH/EntryType.java
