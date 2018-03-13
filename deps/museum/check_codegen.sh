#!/bin/bash
# Copyright 2004-present Facebook. All Rights Reserved.

SRCDIR=$1

if [ ! -f "$SRCDIR/model.yml" ] || [ ! -f "$SRCDIR/gen.cc" ]; then
  echo "Required file(s) for model check missing." >&2
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
  echo "gen.cc is out of date with model.yml. Please re-run //native/museum/run_codegen.sh" >&2
  exit 1
fi
