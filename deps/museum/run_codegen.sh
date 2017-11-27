#!/bin/bash
# Copyright 2004-present Facebook. All Rights Reserved.

art_ver=${1%/}

basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -z "$art_ver" ] || [ ! -d "$basedir/$art_ver" ]; then
  echo "Usage: run_codegen.sh <art_version>" >&2
  exit 1
fi

# === Weird sed invocation at the end??? ===
# Mustache isn't capable of "join"-type logic, where a comma is not added after the last item
# in a list. Thus, it produces function headers that look like:
#   int foo(char p1, char p2, )
# There are a couple different common workarounds for this in mustache file development, but
# none of them work very well for this particular scenario (especially since our list sometimes
# needs prepending with an extra element). So, live with the shitty generation and then just
# use sed to replace ", )" with ")".
#
# Mustache is awful.

cat "$basedir/$art_ver/model.yml"                           \
  | ruby "$basedir/preprocess.rb"                           \
  | mustache - "$basedir/gen_cc.mustache"                   \
  | sed -e "s/, )/)/g"                                      \
  > "$basedir/$art_ver/gen.cc"

"$basedir"/../../scripts/signedsource.py -q sign "$basedir/$art_ver/gen.cc"
