#!/usr/bin/env python

#compsci201
#overgrowninterviewquestions
#ashamedofhowlongthistookme

import sys

if len(sys.argv) != 2 and len(sys.argv) != 3:
    print "Usage: add_header_namespace.py //fully/qualified/buck:target [preferredNamespace[,lessPreferredNamespace[,...]]]"
    print "Target must be a cxx_library."
    quit()

import json
import os
import re
import sets
import time


def do_buck(cmd):
    sys.stdout.write(cmd + " ...")
    sys.stdout.flush()
    start = time.time()
    ret = os.popen(cmd).read()
    print "done in {}s".format(time.time() - start)
    return ret


realTarget = do_buck("buck query " + sys.argv[1] + " 2>/dev/null").strip("\n")
target = json.loads(do_buck("buck targets --json " + realTarget + " 2>/dev/null"))[0]
# TODO: move this back to using except when buck gets fixed
deplist = do_buck("buck query 'deps(" + realTarget + ")' 2>/dev/null").splitlines()
deplist.remove(realTarget)
if len(deplist) > 0:
    deps = json.loads(do_buck("buck targets --json " + " ".join(deplist) + " 2>/dev/null"))
else:
    deps = []
preferredNamespaces = sys.argv[2].split(",") if len(sys.argv) == 3 else []



def addFilesToMod(fileList, fieldData):
    """
    Appends files listed in fieldData to the list tracked by fileList. If fieldData is a dict instead of
    a list, appends the dict's values.
    """
    if isinstance(fieldData, dict):
        fieldData = fieldData.values()
    fileList.extend(fieldData)

filesToMod = []
for field in ['headers', 'srcs', 'exportedHeaders']:
    if field in target:
        addFilesToMod(filesToMod, target[field])
if 'exportedPlatformHeaders' in target:
    for platform in target['exportedPlatformHeaders']:
        addFilesToMod(filesToMod, platform[1])
filesToMod = [item for item in filesToMod if item[0] != ":"]

def headerns(target):
    """
    Returns the header_namespace of a target, suffixed with a /, or empty string if no namespace.
    target: JSON map for a target.
    """
    ns = target['headerNamespace'] if 'headerNamespace' in target else ""
    return ((ns + "/") if len(ns) > 0 else "")
def mergeExportedHeaders(headerMap, tgt, newHeaders):
    """
    Merges exported headers listed in newHeaders into headerMap, multimap-style. A reduce step afterward
    is needed to transform the multimap into a true map.
    """
    if isinstance(newHeaders, dict):
        newHeaders = newHeaders.keys()

    for item in newHeaders:
        if item not in headerMap:
            headerMap[item] = [ headerns(tgt) + item ]
        else:
            headerMap[item].append(headerns(tgt) + item)
def reduceHeaderEntries(raw_header, namespaced_headers):
    """
    Reduces the list namespaced_headers to a single value based on user-specified namespace order preference.
    """
    namespaced_headers = sets.Set(namespaced_headers)
    if len(namespaced_headers) == 1:
        return namespaced_headers.pop()

    for pn in preferredNamespaces:
        if (pn + "/" + raw_header) in namespaced_headers:
            return pn + "/" + raw_header

    raise ValueError(namespaced_headers)

importedHeaderMap = {}
for dep in deps:
    if 'exportedHeaders' in dep:
        mergeExportedHeaders(importedHeaderMap, dep, dep['exportedHeaders'])
    if 'exportedPlatformHeaders' in dep:
        for platform in dep['exportedPlatformHeaders']:
            mergeExportedHeaders(importedHeaderMap, dep, platform[1])
for raw_header, namespaced_headers in importedHeaderMap.items():
    importedHeaderMap[raw_header] = reduceHeaderEntries(raw_header, namespaced_headers)

localHeaderMap = {}
if 'headers' in target:
    mergeExportedHeaders(localHeaderMap, target, target['headers'])
if 'exportedHeaders' in target:
    mergeExportedHeaders(localHeaderMap, target, target['exportedHeaders'])
if 'exportedPlatformHeaders' in target:
    for platform in target['exportedPlatformHeaders']:
        mergeExportedHeaders(localHeaderMap, target, platform[1])
for raw_header, namespaced_headers in localHeaderMap.items():
    localHeaderMap[raw_header] = reduceHeaderEntries(raw_header, namespaced_headers)

headerMap = dict(importedHeaderMap)
headerMap.update(localHeaderMap)
nextMap = { raw: importedHeaderMap[raw] for raw in importedHeaderMap.viewkeys() & localHeaderMap.viewkeys() }

for ftm in filesToMod:
    filename = target['buck.base_path'] + "/" + ftm

    with open(filename, "r") as f:
        lines = f.readlines()

    with open(filename, "w") as f:
        def swap_header(matchobj):
            mapToUse = headerMap if matchobj.group(2) == None else nextMap
            if (os.path.dirname(ftm) + "/" + matchobj.group(3)) in mapToUse:
                return matchobj.group(1).replace("_next", "") + "<" + mapToUse[(os.path.dirname(ftm) + "/" + matchobj.group(3))] + ">"
            elif matchobj.group(3) in mapToUse:
                return matchobj.group(1).replace("_next", "") + "<" + mapToUse[matchobj.group(3)] + ">"
            else:
                return matchobj.group(0)

        for line in lines:
            f.write(re.sub(r'(^\s*#\s*include(_next)?\s+)["<]([^">]+)[">]', swap_header, line))
