#!/bin/bash
# Copyright 2004-present Facebook. All Rights Reserved.

function show_help {
  {
    echo "Converts model.yml files to generated-code trampolines for calling museum methods."
    echo ""
    echo "Usage: run_codegen.sh [-h] [model_dir]"
    echo "  -h        : Show help"
    echo "  model_dir : Directory to search for model.yml files. If not specified, defaults to"
    echo "              directory where this script lives."
    echo ""
    echo "For an overview of the model.yml format, see gen_cc.mustache in run_codegen.sh's directory."
  } >&2
}

if [ "$1" == "-h" ]; then
  show_help
  exit 0
fi

basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ -n "$1" ]; then
  model_dir="$1"
else
  model_dir="$basedir"
fi

[ -d "$model_dir" ] || (show_help; exit 0;)

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

for model in $(find "$model_dir" -name model.yml); do
  ruby "$basedir/preprocess.rb" "$model"                      \
    | mustache - "$basedir/gen_cc.mustache"                   \
    | sed -e "s/, )/)/g"                                      \
    > "$(dirname "$model")/gen.cc"
  "$basedir"/../../scripts/signedsource.py -q sign "$(dirname "$model")/gen.cc"

  ruby "$basedir/preprocess.rb" "$model"                      \
    | mustache - "$basedir/preinit_h.mustache"                \
    | sed -e "s/, )/)/g"                                      \
    > "$(dirname "$model")/preinit.h"
  "$basedir"/../../scripts/signedsource.py -q sign "$(dirname "$model")/preinit.h"

done
