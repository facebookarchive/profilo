The complete flow for tracing-downloading-analyzing is as follows:

1) Trace on the client. An example of how this can be achieved can be found on
   `java/main/com/facebook/loom/sample/SampleAppMainActivity.java`

2) Download the trace on a host. This can be done in a few ways:
   a) Navigate to python/ and run
      `python -m loom.loom pull_last_trace <your_application_package>`

      In this case:

      `python -m loom.loom pull_last_trace com.facebook.loom.sample`

   b) Navigate to python/loom/device, start an interactive Python shell, and
      use the "device" module directly:

      ```
        import device
        device.pull_last_trace('com.example.package.name')
      ```

   Both approaches will result in a file with the same name as the trace to
   be created in the current directory

3) Analyze the data. A sample demo script is provided in
   `python/loom/workflow_demo.py`. It contains logic to perform analysis on
   stacks, blocks, and system counters.

   Stacks:
     Parses the sampled stacks from a trace file into a format similar to that
     of DTrace. This allows for visualizations such as FlameGraph
     https://github.com/brendangregg/FlameGraph

     Note that functions for which we couldn't find symbols will just show up
     as numbers.

   Blocks:
     Parses the instrumented start-end blocks from a trace file and outputs
     some per-thread statistics (mean, median, std) about execution times in
     csv format.

   System counters:
     Parses system counters from a trace file and sorts them by timestamp so
     as to obtain a time series. A command line argument must be used to
     define the output path for the generated graphs.
