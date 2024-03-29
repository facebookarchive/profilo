# Profilo: an Android performance library

## Status

**THIS PROJECT IS CURRENTLY IN MAINTENANCE MODE. It will not receive any feature updates, only critical security bug patches. On May 1st, 2023, the repo will be fully archived.**

--------------------

## Introduction
Profilo is an Android library for collecting performance *traces* from production builds of an app.

## Index

1. [Getting started](https://facebookincubator.github.io/profilo/docs/getting-started)
2. [Internal architecture](https://facebookincubator.github.io/profilo/docs/architecture)
3. [Trace processing and analysis](https://facebookincubator.github.io/profilo/docs/trace-processing)

## APIs

Currently, none of the APIs are stable and they *will* change (mostly so that they're easier to use) before release.

## Sample usage

The sample app in `java/main/com/facebook/profilo/sample` shows the most basic usage of the APIs.

You can also find the prebuilt apk in the Releases section.

## Demos

A demo script can be found in `python/profilo/workflow_demo.py`. It contains examples of simple
analysis that can be run on Profilo traces. For more information see the [trace processing](https://facebookincubator.github.io/profilo/docs/trace-processing.html) section of the docs.

## License
Profilo is Apache 2 licensed, as found in the LICENSE file.
