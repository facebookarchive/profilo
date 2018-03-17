#!/usr/bin/env python

import os
import re
import sys

SUPPORT_HEADERS = [
    "ctype.h",
    "errno.h",
    "iconv.h",
    "inttypes.h",
    "langinfo.h",
    "locale.h",
    "math.h",
    "monetary.h",
    "nl_types.h",
    "stdint.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "time.h",
    "uchar.h",
    "wchar.h",
    "wctype.h",
    "xlocale.h",
]

if len(sys.argv) != 2:
    print "Usage: add_header_guard.py <header_path>"
    quit()
filename = sys.argv[1]


def appendOrRemove(word, suffix):
    if word[-len(suffix):] == suffix:
        return word[:-len(suffix)]
    else:
        return word + suffix


def prependOrRemove(word, prefix):
    if word[:len(prefix)] == prefix:
        return word[len(prefix):]
    else:
        return prefix + word


def genGuardPermutations(guard, filename):
    guards = [guard]
    guards += [prependOrRemove(g, "_") for g in guards]
    guards += [appendOrRemove(g, "_") for g in guards]
    if os.path.basename(filename) in SUPPORT_HEADERS:
        guards += ["NDK_ANDROID_SUPPORT" + g for g in guards if g[0] == "_"]
    if "/uapi/" in filename:
        guards += [prependOrRemove(g, "_UAPI") for g in guards if g[0] == "_"]
    return guards


with open(filename, "r") as f:
    lines = f.readlines()

guard = None
with open(filename, "w") as f:
    for line in lines:
        f.write(line)
        if guard is None:
            m = re.match("#ifndef ([A-Z_]+_H_?)$", line)
            if m is not None:
                guard = m.group(1)
        # note, must not simply test guard for none-ness here - might get reset
        else:
            m = re.match("#define " + guard, line)
            if m is not None:
                for newGuard in genGuardPermutations(guard, filename):
                    newGuardLine = "#define " + newGuard
                    if all(newGuardLine not in line for line in lines):
                        f.write(newGuardLine + "\n")
            guard = None
