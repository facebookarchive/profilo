/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.facebook.profilo.provider.stacktrace;

import android.content.Context;
import android.os.Build;
import android.os.Process;
import com.facebook.profilo.logger.MultiBufferLogger;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

@DoNotStrip
public class CPUProfiler {

  public static final int DEFAULT_PROFILER_SAMPLING_RATE_MS = 23; // TODO_YM T74020064
  public static final int DEFAULT_THREAD_DETECT_INTERVAL_MS = 23;

  private static volatile boolean sInitialized = false;
  private static volatile int sAvailableTracers = 0;

  static {
    SoLoader.loadLibrary("profilo_stacktrace");
  }

  /* Keep in sync with BaseTracer in C++ */
  public static final int TRACER_DALVIK = 1;
  public static final int TRACER_NATIVE = 1 << 2;
  public static final int TRACER_ART_UNWINDC_6_0 = 1 << 4;
  public static final int TRACER_ART_UNWINDC_7_0_0 = 1 << 5;
  public static final int TRACER_ART_UNWINDC_7_1_0 = 1 << 6;
  public static final int TRACER_ART_UNWINDC_7_1_1 = 1 << 7;
  public static final int TRACER_ART_UNWINDC_7_1_2 = 1 << 8;
  public static final int TRACER_JAVASCRIPT = 1 << 9;
  public static final int TRACER_ART_UNWINDC_5_0 = 1 << 10;
  public static final int TRACER_ART_UNWINDC_5_1 = 1 << 11;
  public static final int TRACER_ART_UNWINDC_8_0_0 = 1 << 12;
  public static final int TRACER_ART_UNWINDC_8_1_0 = 1 << 13;
  public static final int TRACER_ART_UNWINDC_9_0_0 = 1 << 14;

  private static int calculateTracers(Context context) {
    int tracers = 0;

    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
      tracers |= TRACER_DALVIK;
    } else if (ArtCompatibility.isCompatible(context)) {
      switch (Build.VERSION.RELEASE) {
        case "9":
        case "9.0":
        case "9.0.0":
          tracers |= TRACER_ART_UNWINDC_9_0_0;
          break;
        case "8.1.0":
          tracers |= TRACER_ART_UNWINDC_8_1_0;
          break;
        case "8.0.0":
          tracers |= TRACER_ART_UNWINDC_8_0_0;
          break;
        case "7.1.2":
          tracers |= TRACER_ART_UNWINDC_7_1_2;
          break;
        case "7.1.1":
          tracers |= TRACER_ART_UNWINDC_7_1_1;
          break;
        case "7.1":
        case "7.1.0":
          tracers |= TRACER_ART_UNWINDC_7_1_0;
          break;
        case "7.0":
          tracers |= TRACER_ART_UNWINDC_7_0_0;
          break;
        case "6.0":
        case "6.0.1":
          tracers |= TRACER_ART_UNWINDC_6_0;
          break;
        case "5.1":
        case "5.1.0":
        case "5.1.1":
          tracers |= TRACER_ART_UNWINDC_5_1;
          break;
        case "5.0":
        case "5.0.1":
        case "5.0.2":
          tracers |= TRACER_ART_UNWINDC_5_0;
          break;
      }
    }
    tracers |= TRACER_JAVASCRIPT;
    if (Build.VERSION.SDK_INT >= 26 /* 8.0.0 */) {
      tracers |= TRACER_NATIVE;
    }

    return tracers;
  }

  static synchronized int getAvailableTracers() {
    return sAvailableTracers;
  }

  public static synchronized boolean init(
      Context context,
      MultiBufferLogger logger,
      boolean nativeTracerUnwindDexFrames,
      boolean nativeTracerUnwindJitFrames,
      int nativeTracerUnwinderThreadPriority,
      int nativeTracerUnwinderQueueSize,
      boolean nativeTracerLogPartialStacks)
      throws Exception {
    if (sInitialized) {
      return true;
    }

    sAvailableTracers = calculateTracers(context);
    sInitialized =
        nativeInitialize(
            logger,
            sAvailableTracers,
            nativeTracerUnwindDexFrames,
            nativeTracerUnwindJitFrames,
            nativeTracerUnwinderThreadPriority,
            nativeTracerUnwinderQueueSize,
            nativeTracerLogPartialStacks);
    return sInitialized;
  }

  public static synchronized void stopProfiling() {
    if (!sInitialized) {
      return;
    }
    nativeStopProfiling();
  }

  public static synchronized boolean startProfiling(
      int requestedTracers,
      int samplingRateMs,
      int threadDetectIntervalMs,
      boolean cpuClockModeEnabled,
      boolean wallClockModeEnabled) {
    if (!cpuClockModeEnabled && !wallClockModeEnabled) {
      return false;
    }
    // We always trace the main thread.
    StackTraceWhitelist.add(Process.myPid());

    return sInitialized
        && nativeStartProfiling(
            requestedTracers,
            samplingRateMs,
            threadDetectIntervalMs,
            cpuClockModeEnabled,
            wallClockModeEnabled);
  }

  public static void loggerLoop() {
    if (!sInitialized) {
      return;
    }
    nativeLoggerLoop();
  }

  public static void resetFrameworkNamesSet() {
    if (!sInitialized) {
      return;
    }
    nativeResetFrameworkNamesSet();
  }

  /** Note: Init correctness is guaranteed for Main Thread only at this point. */
  @DoNotStrip
  private static native boolean nativeInitialize(
      MultiBufferLogger logger,
      int availableTracers,
      boolean nativeTracerUnwindDexFrames,
      boolean nativeTracerUnwindJitFrames,
      int nativeTracerUnwinderThreadPriority,
      int nativeTracerUnwinderQueueSize,
      boolean nativeTracerLogPartialStacks);

  @DoNotStrip
  private static native boolean nativeStartProfiling(
      int providers,
      int samplingRateMs,
      int threadDetectIntervalMs,
      boolean cpuClockModeEnabled,
      boolean wallClockModeEnabled);

  @DoNotStrip
  private static native void nativeStopProfiling();

  @DoNotStrip
  private static native void nativeLoggerLoop();

  @DoNotStrip
  private static native void nativeResetFrameworkNamesSet();
}
