#!/bin/bash

## THIS IS SUPPOSED TO RUN INSIDE THE CONTAINER

set -e

ln -s buck_imports/buckconfig .buckconfig
buck fetch deps/...
buck build aar sample

AAR=$(buck targets --show-output aar |& tail -n 1 | cut -f2 -d' ')
APK=$(buck targets --show-output sample |& tail -n 1 | cut -f2 -d' ')

OUT="/out"
mkdir -p "$OUT"
cp "$AAR" "$OUT"/profilo.aar
cp "$APK" "$OUT"/sample.apk
