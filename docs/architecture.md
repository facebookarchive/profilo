---
id: architecture
title: Architecture
sidebar_label: Architecture
---

## Glossary

* **Trace**: a file containing a time window of performance data from an application.
* **Trigger**: an attempt to start a trace.
* **TraceController**: an object controlling whether Profilo should actually start a trace. Each Trigger is associated with one TraceController.
* **TraceProvider**: an object providing a subset of data in the trace. Examples are stack traces, systrace (atrace), system counters, etc. Different providers can be enabled in different traces, all at the discretion of the TraceController.
* **ConfigProvider**: an object providing a Config implementation, which **TraceController** can check to determine whether to satisfy the request.


## Fundamental design concerns

There are a few concerns that drive most of Profilo's design.

1. The ability to configure separate layers of data (providers) inside the collected traces.
2. The ability to decouple "trace requests" from "trace configuration".
3. The ability to efficiently write data into the trace file.

Let's go through each in turn to investigate how it was handled.

### Trace Providers

Trace providers are fundamentally just bits in a bitset. The bitset is managed by `TraceEvents` while the bits are assigned by `ProvidersRegistry`. Registering a provider requires that you associate it with a name.

This name may appear in a config (see below) and the numeric constant (single bit in the bitset) may be used directly to check if the provider is enabled in the trace.

The `TraceProvider` interface (and `BaseTraceProvider` abstract class) will receive notification at the beginning and end of a trace to perform setup and teardown work.

It's easy to get the interactions between registering and using providers wrong, so we recommend this pattern for your custom data needs:

```java
public final class MyProvider extends BaseTraceProvider {
  public static final int PROVIDER_MY_PROVIDER =
      ProvidersRegistry.newProvider("my_provider");

  @Override
  public void enable() {}

  @Override
  public void disable() {}

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_MY_PROVIDER;
  }

  @Override
  protected int getTracingProviders() {
    return PROVIDER_MY_PROVIDER;
  }
}
```

## Trace configuration

**TraceControllers** can use external configuration to determine whether to allow a trace request.

An example may be a hypothetical trigger `STRING_TRIGGER` with the following associated controller:

```java
public class StringController implements TraceController {

  @Override
  public int evaluateConfig(@Nullable Object context, ControllerConfig config) {
    // Downcast to the types we expect.
    String str = (String) context;
    StringControllerConfig strconfig = (StringControllerConfig) config;

    return strconfig.hasKey(str);
  }

  @Override
  public int getCpuSamplingRateMs(Object context, ControllerConfig config) {
    StringControllerConfig strconfig = (StringControllerConfig) config;

    return strconfig.get(str).cpuSamplingRateMs;
  }

  @Override
  public boolean contextsEqual(int fstInt, @Nullable Object fst, int sndInt, @Nullable Object snd) {
    if ((fst == null && snd != null) ||
      (fst != null && snd == null)) {
        return false;
    }
    return fst == snd || fst.equals(snd);
  }

  public boolean isConfigurable() {
    return true;
  }
```

This controller can then be used as follows:
```java
TraceControl.get().startTrace(
  STRING_TRIGGER,
  0, // flags
  0, // intContext
  "my_unique_string_identifying_this_trace_request");
```

It is the responsibility of your custom `ConfigProvider` to return an instance of `StringControllerConfig` when asked for the controller config for `STRING_TRIGGER`.

You can set your `ConfigProvider` at any point, Profilo takes care to ensure that the config presented to `TraceControllers` is consistent while a trace is active.

## Adding events to the trace

**NOTE:** Subject to imminent change.

The current APIs to log data into the trace are `com.facebook.profilo.logger.Logger` (Java) and `facebook::profilo::logger::Logger` (C++).

They're not very extensible but we're working on fixing the APIs and data formats. Until then, you can look at how the builtin providers use them.
