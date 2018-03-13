# Profilo: an Android performance library

## Introduction
Profilo is an Android library for collecting performance *traces* from production builds of an app.

## Index

1. [Getting started](docs/getting-started.md)
2. [Internal architecture](docs/architecture.md)
3. [Trace processing and analysis](docs/trace-processing.md)

## APIs

Currently, none of the APIs are stable and they *will* change (mostly so that they're easier to use) before release.

## Sample usage

The sample app in `java/main/com/facebook/profilo/sample` shows the most basic usage of the APIs.

You can also find the prebuilt apk in the Releases section.

## Demos

A demo script can be found in `python/profilo/workflow_demo.py`. It contains examples of simple
analysis that can be run on Profilo traces. For more information see the README
`python/demos/README.md`
