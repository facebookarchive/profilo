#!/bin/bash

set -e
set -o pipefail
ROOT=$(buck root)
OUTPUT_PATH="$ROOT/libraries/profilo/cpp/generated"
CODEGEN_TARGET="//libraries/profilo/cpp/codegen:codegen"

buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_types > $OUTPUT_PATH/EntryType.h
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_types_cpp > $OUTPUT_PATH/EntryType.cpp
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_structs > $OUTPUT_PATH/Entry.h
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode entry_structs_cpp > $OUTPUT_PATH/Entry.cpp
buck run ${CODEGEN_TARGET} -- --lang cpp --module android --mode parser > $OUTPUT_PATH/EntryParser.h

buck run ${CODEGEN_TARGET} -- --lang java --module android --mode entry_types > $OUTPUT_PATH/EntryType.java
