#!/bin/bash
# Copyright 2004-present Facebook. All Rights Reserved.

SRCDIR=$1

if [ ! -f "$SRCDIR/model.yml" ] && [ ! -f "$SRCDIR/gen.cc" ] && [ ! -f "$SRCDIR/preinit.h" ]; then
  # if *none* of the files exist, then this target simply doesn't have a model and this check is a no-op
  exit 0
fi

if [ ! -f "$SRCDIR/model.yml" ]; then
  echo "model.yml missing" >&2
  exit 127
fi
if [ ! -f "$SRCDIR/gen.cc" ]; then
  echo "gen.cc missing" >&2
  exit 127
fi
if [ ! -f "$SRCDIR/preinit.h" ]; then
  echo "preinit.h missing" >&2
  exit 127
fi

case "$(uname)" in
  "Darwin")
    modelmd5=$(md5 -q $SRCDIR/model.yml)
    ;;
  "Linux")
    modelmd5=$(md5sum $SRCDIR/model.yml | cut -d ' ' -f 1)
    ;;
  *)
    echo "Only OS X and Linux supported.">&2
    exit 1
  ;;
esac

if [ "$modelmd5" != "$(grep @model $SRCDIR/gen.cc | cut -d ' ' -f 4)" ]; then
  echo "gen.cc is out of date with model.yml. Please re-run \`//native/museum/run_codegen.sh $(basename "$SRCDIR")\`" >&2
  exit 1
fi
if [ "$modelmd5" != "$(grep @model $SRCDIR/preinit.h | cut -d ' ' -f 4)" ]; then
  echo "preinit.h is out of date with model.yml. Please re-run \`//native/museum/run_codegen.sh $(basename "$SRCDIR")\`" >&2
  exit 1
fi
