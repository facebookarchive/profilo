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

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.Context;
import android.os.Process;
import android.util.Log;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.Identifiers;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.MultiBufferLogger;
import com.facebook.proguard.annotations.DoNotStrip;
import java.util.Locale;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

public final class StackFrameThread extends BaseTraceProvider {
  public static final int PROVIDER_STACK_FRAME = ProvidersRegistry.newProvider("stack_trace");
  public static final int PROVIDER_WALL_TIME_STACK_TRACE =
      ProvidersRegistry.newProvider("wall_time_stack_trace");
  public static final int PROVIDER_NATIVE_STACK_TRACE =
      ProvidersRegistry.newProvider("native_stack_trace");

  private static final String LOG_TAG = "StackFrameThread";

  private enum TimeSource {
    NONE,
    WALL,
    CPU,
    BOTH
  };

  @GuardedBy("this")
  private @Nullable Thread mProfilerThread;

  @GuardedBy("this")
  private final Context mContext;

  private volatile boolean mEnabled;
  private int mSystemClockTimeIntervalMs = -1;
  @Nullable private TraceContext mSavedTraceContext;

  public StackFrameThread(Context context) {
    super("profilo_stacktrace");
    Context applicationContext = context.getApplicationContext();
    if (applicationContext == null && context instanceof Application) {
      // When the context is passed in from com.facebook.katana.app.FacebookApplication
      // getApplicationContext returns null
      mContext = context;
    } else {
      mContext = applicationContext;
    }
  }

  /** Calculate all possible CPUProfiler tracers that correspond to the given providers bitset. */
  private static int providersToTracers(int providers) {
    int tracers = 0;
    if ((providers & (PROVIDER_STACK_FRAME | PROVIDER_WALL_TIME_STACK_TRACE)) != 0) {
      tracers |=
          CPUProfiler.TRACER_DALVIK
              | CPUProfiler.TRACER_ART_UNWINDC_6_0
              | CPUProfiler.TRACER_ART_UNWINDC_7_0_0
              | CPUProfiler.TRACER_ART_UNWINDC_7_1_0
              | CPUProfiler.TRACER_ART_UNWINDC_7_1_1
              | CPUProfiler.TRACER_ART_UNWINDC_7_1_2
              | CPUProfiler.TRACER_JAVASCRIPT
              | CPUProfiler.TRACER_ART_UNWINDC_5_0
              | CPUProfiler.TRACER_ART_UNWINDC_5_1
              | CPUProfiler.TRACER_ART_UNWINDC_8_0_0
              | CPUProfiler.TRACER_ART_UNWINDC_8_1_0
              | CPUProfiler.TRACER_ART_UNWINDC_9_0_0;
    }
    if ((providers & PROVIDER_NATIVE_STACK_TRACE) != 0) {
      tracers |= CPUProfiler.TRACER_NATIVE;
    }
    return tracers;
  }

  /**
   * Inits {@link CPUProfiler} and the profiler handler.
   *
   * @return <code>true</code> if initProfiler was successful and <code>false</code> otherwise.
   */
  private synchronized boolean initProfiler(
      boolean nativeTracerUnwindDexFrames,
      boolean nativeTracerUnwindJitFrames,
      int nativeTracerUnwinderThreadPriority,
      int nativeTracerUnwinderQueueSize,
      boolean nativeTracerLogPartialStacks) {
    try {
      return CPUProfiler.init(
          mContext,
          getLogger(),
          nativeTracerUnwindDexFrames,
          nativeTracerUnwindJitFrames,
          nativeTracerUnwinderThreadPriority,
          nativeTracerUnwinderQueueSize,
          nativeTracerLogPartialStacks);
    } catch (Exception ex) {
      Log.e(LOG_TAG, ex.getMessage(), ex);
      return false;
    }
  }

  private synchronized boolean enableInternal(
      int sampleRateMs,
      int threadDetectIntervalMs,
      int enabledProviders,
      boolean nativeTracerUnwindDexFrames,
      boolean nativeTracerUnwindJitFrames,
      int nativeTracerUnwinderThreadPriority,
      int nativeTracerUnwinderQueueSize,
      TimeSource timeSource,
      boolean nativeTracerLogPartialStacks) {
    if (!initProfiler(
        nativeTracerUnwindDexFrames,
        nativeTracerUnwindJitFrames,
        nativeTracerUnwinderThreadPriority,
        nativeTracerUnwinderQueueSize,
        nativeTracerLogPartialStacks)) {
      return false;
    }

    if (sampleRateMs <= 0) {
      sampleRateMs = CPUProfiler.DEFAULT_PROFILER_SAMPLING_RATE_MS;
    }
    if (threadDetectIntervalMs <= 0) {
      threadDetectIntervalMs = CPUProfiler.DEFAULT_THREAD_DETECT_INTERVAL_MS;
    }

    // Default to get stack traces from all threads, override for wall time
    // stack profiling of main thread on-demand.

    boolean wallClockModeEnabled = false;
    boolean cpuClockModeEnabled = false;
    if ((enabledProviders & PROVIDER_WALL_TIME_STACK_TRACE) != 0) {
      wallClockModeEnabled = true;
    } else {
      switch (timeSource) {
        case BOTH:
          wallClockModeEnabled = true;
          cpuClockModeEnabled = true;
          break;
        case WALL:
          wallClockModeEnabled = true;
          break;
        case NONE:
        case CPU:
          cpuClockModeEnabled = true;
          break;
      }
    }
    // For now, we'll just keep an eye on the main thread. Eventually we
    // might want to pass a list of all the interesting threads.
    boolean started =
        CPUProfiler.startProfiling(
            providersToTracers(enabledProviders),
            sampleRateMs,
            threadDetectIntervalMs,
            cpuClockModeEnabled,
            wallClockModeEnabled);
    if (!started) {
      return false;
    }

    getLogger()
        .writeStandardEntry(
            Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.TRACE_ANNOTATION,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            Identifiers.CPU_SAMPLING_INTERVAL_MS,
            ProfiloConstants.NONE,
            (long) sampleRateMs);

    mEnabled = true;
    return mEnabled;
  }

  private TimeSource parseTimeSourceParam(@Nullable String param) {
    if (param == null || param.length() == 0) {
      return TimeSource.NONE;
    }

    try {
      TimeSource timeSource = TimeSource.valueOf(param.toUpperCase(Locale.US));
      return timeSource;
    } catch (IllegalArgumentException ex) {
      Log.e(LOG_TAG, ex.getMessage(), ex);
    }

    return TimeSource.NONE;
  }

  @Override
  @SuppressLint({
    "BadMethodUse-java.lang.Thread.start",
  })
  protected void enable() {
    final TraceContext context = getEnablingTraceContext();
    // Avoid starting a thread if there's nothing to do.
    if (providersToTracers(context.enabledProviders) == 0) {
      return;
    }

    if (mProfilerThread != null) {
      Log.e(LOG_TAG, "Duplicate attempt to enable sampling profiler.");
      return;
    }

    TimeSource timeSource =
        parseTimeSourceParam(
            context.mTraceConfigExtras.getStringParam(ProfiloConstants.TIME_SOURCE_PARAM, null));
    boolean enabled =
        enableInternal(
            context.mTraceConfigExtras.getIntParam(
                ProfiloConstants.CPU_SAMPLING_RATE_CONFIG_PARAM, 0),
            context.mTraceConfigExtras.getIntParam(
                ProfiloConstants.PROVIDER_PARAM_STACK_TRACE_THREAD_DETECT_INTERVAL_MS, 0),
            context.enabledProviders,
            context.mTraceConfigExtras.getBoolParam(
                ProfiloConstants.PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWIND_DEX_FRAMES, false),
            context.mTraceConfigExtras.getBoolParam(
                ProfiloConstants.PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWIND_JIT_FRAMES, true),
            context.mTraceConfigExtras.getIntParam(
                ProfiloConstants.PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWINDER_THREAD_PRIORITY,
                ProfiloConstants.TRACE_CONFIG_PARAM_LOGGER_PRIORITY_DEFAULT),
            context.mTraceConfigExtras.getIntParam(
                ProfiloConstants.PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWINDER_QUEUE_SIZE,
                ProfiloConstants.PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWINDER_QUEUE_SIZE_DEFAULT),
            timeSource,
            context.mTraceConfigExtras.getBoolParam(
                ProfiloConstants.PROVIDER_PARAM_NATIVE_STACK_TRACE_LOG_PARTIAL_STACKS, false));
    if (!enabled) {
      return;
    }
    mSavedTraceContext = context;

    mProfilerThread =
        new Thread(
            new Runnable() {
              public void run() {
                Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT);
                try {
                  CPUProfiler.loggerLoop();
                } catch (Exception ex) {
                  Log.e(LOG_TAG, ex.getMessage(), ex);
                }
              }
            },
            "Prflo:Profiler");
    mProfilerThread.start();
  }

  @Override
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    if ((context.enabledProviders & (PROVIDER_STACK_FRAME | PROVIDER_WALL_TIME_STACK_TRACE)) != 0) {
      logAnnotation(
          "provider.stack_trace.art_compatibility",
          Boolean.toString(ArtCompatibility.isCompatible(mContext)));
      logAnnotation(
          "provider.stack_trace.tracers",
          Integer.toBinaryString(
              providersToTracers(context.enabledProviders) & CPUProfiler.getAvailableTracers()));
    }
    if ((context.enabledProviders & getSupportedProviders()) != 0) {
      logAnnotation(
          "provider.stack_trace.cpu_timer_res_us",
          Integer.toString(nativeCpuClockResolutionMicros()));
    }
  }

  private void logAnnotation(String key, String value) {
    MultiBufferLogger logger = getLogger();
    int match =
        logger.writeStandardEntry(
            Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.TRACE_ANNOTATION,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            ProfiloConstants.NO_MATCH,
            ProfiloConstants.NONE);
    match = logger.writeBytesEntry(ProfiloConstants.NONE, EntryType.STRING_KEY, match, key);
    logger.writeBytesEntry(ProfiloConstants.NONE, EntryType.STRING_VALUE, match, value);
  }

  @Override
  protected void disable() {
    if (!mEnabled) {
      mProfilerThread = null;
      return;
    }
    mSavedTraceContext = null;
    mEnabled = false;
    CPUProfiler.stopProfiling();
    if (mProfilerThread != null) {
      try {
        mProfilerThread.join();
      } catch (InterruptedException ex) {
        throw new RuntimeException(ex);
      }
      mProfilerThread = null;
    }
  }

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_NATIVE_STACK_TRACE | PROVIDER_STACK_FRAME | PROVIDER_WALL_TIME_STACK_TRACE;
  }

  @Override
  protected int getTracingProviders() {
    TraceContext savedTraceContext = mSavedTraceContext;
    if (!mEnabled || savedTraceContext == null) {
      return 0;
    }

    int tracingProviders = 0;
    int enabledProviders = savedTraceContext.enabledProviders;
    if ((enabledProviders & PROVIDER_WALL_TIME_STACK_TRACE) != 0) {
      tracingProviders |= PROVIDER_WALL_TIME_STACK_TRACE;
    } else if ((enabledProviders & PROVIDER_STACK_FRAME) != 0) {
      tracingProviders |= PROVIDER_STACK_FRAME;
    }
    tracingProviders |= enabledProviders & PROVIDER_NATIVE_STACK_TRACE;
    return tracingProviders;
  }

  @Override
  protected void onTraceStarted(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    CPUProfiler.resetFrameworkNamesSet();
  }

  @DoNotStrip
  private static native int nativeCpuClockResolutionMicros();
}
