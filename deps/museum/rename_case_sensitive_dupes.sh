#!/bin/bash

basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

case "$(uname -s)" in
  "Linux")
    uniqopts="Di"
    ;;
  "Darwin")
    uniqopts="di"
    ;;
  *)
    echo "unsupported os type; cannot determine correct options to uniq" >&2
    exit 1
    ;;
esac

while read -r filename; do
  hg mv "${filename}" "${filename%.*}_$([[ "$filename" = *.* ]] && echo ".${filename##*.}" || echo '')"
done < <(find "${basedir}" -type f | sort | uniq -${uniqopts} | grep -E "[A-Z]")
