// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.stacktrace;

import android.content.Context;
import android.os.Build;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

@DoNotStrip
public class CPUProfiler {

  public static final int DEFAULT_PROFILER_SAMPLING_RATE_MS = 11;

  private static volatile boolean sInitialized = false;
  private static volatile int sAvailableTracers = 0;

  static {
    SoLoader.loadLibrary("profilo_stacktrace");
  }

  /* Keep in sync with BaseTracer in C++ */
  public static final int TRACER_DALVIK = 1;
  public static final int TRACER_ART_6_0 = 1 << 1;
  public static final int TRACER_NATIVE = 1 << 2;
  public static final int TRACER_ART_5_1 = 1 << 3;
  public static final int TRACER_ART_7_0 = 1 << 4;

  private static int calculateTracers(Context context) {
    int tracers = 0;

    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
      tracers |= TRACER_DALVIK;
    } else if (ArtCompatibility.isCompatible(context)) {
      switch (Build.VERSION.RELEASE) {
        case "7.0":
          tracers |= TRACER_ART_7_0;
          break;
        case "6.0":
        case "6.0.1":
          tracers |= TRACER_ART_6_0;
          break;
        case "5.1":
        case "5.1.0":
        case "5.1.1":
          tracers |= TRACER_ART_5_1;
      }
    }

    String arch = System.getProperty("os.arch");
    // Native tracing is only available on ARM. Assuming ARM by default.
    if (arch == null || arch.startsWith("arm")) {
      tracers |= TRACER_NATIVE;
    }

    return tracers;
  }

  public static synchronized boolean init(Context context) throws Exception {
    if (sInitialized) {
      return true;
    }

    sAvailableTracers = calculateTracers(context);
    sInitialized = nativeInitialize(sAvailableTracers);
    return sInitialized;
  }


  public static synchronized void stopProfiling() {
    if (!sInitialized) {
      return;
    }
    nativeStopProfiling();
  }

  public static synchronized boolean startProfiling(
      int requestedTracers, int samplingRateMs, int targetThread) {
    return sInitialized && nativeStartProfiling(requestedTracers, samplingRateMs, targetThread);
  }

  public static void loggerLoop() {
    if (!sInitialized) {
      return;
    }
    nativeLoggerLoop();
  }

  /**
   * Note: Init correctness is guaranteed for Main Thread only at this point.
   */
  @DoNotStrip
  private static native boolean nativeInitialize(int availableTracers);

  @DoNotStrip
  private static native boolean nativeStartProfiling(
      int providers, int samplingRateMs, int targetThread);

  @DoNotStrip
  private static native void nativeStopProfiling();

  @DoNotStrip
  private static native void nativeLoggerLoop();
}
