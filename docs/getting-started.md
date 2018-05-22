---
id: getting-started
title: Getting Started
sidebar_label: Getting Started
---

## Use a prebuilt `.aar` with Gradle

1. Download a prebuilt `.aar` from the Releases tab above.

  1.1. Every commit that has a successful CI build will produce a GitHub Release.

  1.2. At this point in time, we don't version the library explicitly but this will change in the near future.

2. Add `profilo.aar` to a `libs/` folder under your project.

3. Ensure your `build.gradle` contains the following clause, inside `dependencies`:

```
implementation fileTree(dir: 'libs', include: ['*.jar', '*.aar'])
```

**NOTE**: We are actively working on providing a Gradle-native build and Maven Central artifacts. The GitHub Releases of `.aar`s will stop at that point.

## Start and stop a traces

First, we need to initialize the library by specifying which controllers and providers we'd like to use. (see [Architecture](architecture.md) for details).

This is best done inside your Application object, for example in `onCreate`.

```java
try {
  // SoLoader is a robust native code loading library.
  SoLoader.init(this.getApplicationContext(), 0);
} catch (IOException e) {
  throw new RuntimeException(e);
}

SparseArray<TraceController> controllers = new SparseArray<>(1);

// We want the "ExternalController" with the id specified by TRIGGER_EXTERNAL.
controllers.put(ExternalController.TRIGGER_EXTERNAL, new ExternalController());

TraceOrchestrator.TraceProvider[] providers = new TraceOrchestrator.TraceProvider[] {
    new SystemCounterThread(),
    new StackFrameThread(context),
    new SystraceProvider(),
    new ThreadMetadataProvider(),
};

TraceOrchestrator.initialize(
    this.getApplicationContext(),
    /* configProvider */ null,  // we won't be using the remote configurability in this example
    TraceOrchestrator.MAIN_PROCESS_NAME,
    /* isMainProcess */ true,
    providers,
    controllers);

// Optional, will get called when there's a trace
// to be uploaded from this app. Without it, traces
// will be rotated and kept on device.
TraceOrchestrator
  .get()
  .setBackgroundUploadService(new MyUploadService());
```

Once the library is initialized, we can make start/stop trace requests via `ExternalTraceControl`:

```java
ExternalTraceControl.startTrace(
    StackFrameThread.PROVIDER_STACK_FRAME
    | SystemCounterThread.PROVIDER_SYSTEM_COUNTERS
    | SystraceProvider.PROVIDER_ATRACE,
    /*cpuSamplingRateMs*/ 10);
/* some time later */
ExternalTraceControl.stopTrace();
```

To learn more about controllers, providers and configuration, see [Architecture](architecture.md).

## Building from source
Profilo currently builds via [buck](https://buckbuild.com). This is necessary due to a large dependency graph and a non-trivial amount of native code.

The build is made slightly easier by `build/Dockerfile`.

The following steps closely resemble the Travis CI setup:

```
# Build a docker image with Android toolchains and buck checked out and compiled and the repository copied into the image
docker build -t profilo:16.10 -f build/Dockerfile .

# Build profilo by invoking buck
docker run -t profilo:16.10 /usr/bin/env TERM=dumb bash /repo/build/build.sh

# Copy artifacts from the last container to run to the host
build/copy_artifacts_to_host.sh

# List artifacts
ls out/*
```

TODO:
* Migrate build to Gradle + Maven Central artifacts
* Run tests in CI
