---
id: trace-processing
title: Trace Processing
sidebar_label: Trace Processing
---

## Design

Profilo comes with a Python library which can parse traces into a semantic trace object. This trace object can then be traversed and examined to extract meaningful metrics.

## Trace model

The semantic trace objects follow a hierarchy (or trace model):

*Traces* consist of execution units.

*Execution units* consist of *blocks*. Execution units are usually individual threads in Profilo traces.

*Blocks* consist of *points*. A block always has a start and end point. Points within a block can share timestamps but are uniquely identifiable.

*Edges* can indicate change of control flow between any two points. There's an implicit edge between the start and end points of a block, as well as between the boundary points of consecutive blocks in an execution unit. All other edges must be explicitly created.

All of the above objects can have "annotations" (props in API) attached to them. Annotations are key-value pairs with very loose typing.

```
--Execution Unit 1--------------------

  Block A      Block B
[s.-->  .e]-->[s --> .e]
  |     ^            |
  v     |            |
  [s-->e]            |
  Block C            |
                     |
---------------------|----------------
                     |
--Execution Unit 2---|----------------
                     |
                     v Block D
                     [s --> e]

--------------------------------------
```
In the diagram above, the edges are as follows:

|Edge Source|Edge Target|Type|
|-----------|-----------|----|
|Block A, start point|Block A, end point|Implicit|
|Block B, start point|Block B, end point|Implicit|
|Block C, start point|Block C, end point|Implicit|
|Block D, start point|Block D, end point|Implicit|
|Block A, 2nd point| Block C, start point|Explicit, child call|
|Block C, end point|Block A, 3rd point|Explicit, child return|
|Block B, 2nd point|Block D, start point|Explicit, async call|

## Workflow

The complete flow for tracing-downloading-analyzing is as follows:

0. Ensure you have a environment which contains the dependencies listed in `python/requirements.txt`. For example, you can use [virtualenv](https://github.com/pypa/virtualenv) like so:

  ```
  virtualenv -p python3 env
  source env/bin/activate
  pip3 install -r requirements.txt
  ```

1. Start and stop a trace in the app. (You can use the sample app to play with traces)

2. Download the trace on a host. This can be done in a few ways:

   a. Navigate to python/ and run
      `python3 -m profilo.profilo pull_last_trace <your_application_package>`

      In this case:

      `python3 -m profilo.profilo pull_last_trace com.facebook.profilo.sample`

   b. Navigate to `python/profilo/device`, start an interactive Python shell, and
      use the "device" module directly:

      ```
import device
device.pull_last_trace('com.example.package.name')
      ```

   Both approaches will result in a file with the same name as the trace to be created in the current directory.

3. Analyze the data. A sample demo script is provided in `python/profilo/workflow_demo.py`.

   You can run from within the `python/` folder via `python3 -m profilo.workflow_demo --help`. Don't forget to activate your virtualenv first, if you are using one.

   The workflow demo contains logic to perform analysis on stacks, blocks, and system counters.

  a. Stacks:
     Parses the sampled stacks from a trace file into a format similar to that of DTrace. This allows for visualizations such as [FlameGraph](https://github.com/brendangregg/FlameGraph).

     Note that functions for which we couldn't find symbols will just show up as numbers.

   b. Blocks:
     Parses the instrumented start-end blocks from a trace file and outputs some per-thread statistics (mean, median, std) about execution times in
     csv format.

   c. System counters:
     Parses system counters from a trace file and sorts them by timestamp so as to obtain a time series. A command line argument must be used to
     define the output path for the generated graphs.
