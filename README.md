# Loom: an Android performance library

## Introduction
Loom is an Android library for collecting performance *traces* from production builds of an app.

## Building
Loom currently builds via buck only. This is necessary due to a large and complicated amount of native code.

The build is made slightly easier by `build/Dockerfile`. The container is still work in progress and is not *entirely* reproducible.

The following steps closely resemble the Travis CI setup:
```
# Build a docker image with Android toolchains and buck checked out and compiled and the repository copied into the image
docker build -t loom:16.10 -f build/Dockerfile .

# Build loom by invoking buck
docker run -t loom:16.10 /usr/bin/env TERM=dumb bash /repo/build/build.sh

# Copy artifacts from the last container to run to the host
build/copy_artifacts_to_host.sh

# List artifacts
ls out/*
```

The resulting artifacts are also published as Github Releases, from every commit to the repo.

TODO:
* Produce loom-sources.jar, loom-javadoc.jar
* Run tests in Docker/Travis

## Glossary

* Trace: a file containing a time window of performance data from an app
* Trigger: an attempt to start a trace
* TraceController: an object controlling whether Loom should actually start a trace
* TraceProvider: an object providing a subset of data in the trace. Examples are stack traces, systrace, system counters, etc. Different providers can be enabled in different traces, all at the discretion of the TraceController.

## APIs

Currently, none of the APIs are stable and they *will* change (mostly so that they're easier to use) before release.

## Sample usage

The sample app in `java/main/com/facebook/loom/sample` shows the most basic usage of the APIs.

You can also find these apks in the Releases section.

## Demos

A demo script can be found in `python/loom/workflow_demo.py`. It contains examples of simple
analysis that can be run on Loom traces. For more information see the README
`python/demos/README.md`
