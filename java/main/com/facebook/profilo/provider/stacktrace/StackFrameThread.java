/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
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
import com.facebook.proguard.annotations.DoNotStrip;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

public final class StackFrameThread extends BaseTraceProvider {

  public static final int PROVIDER_STACK_FRAME = ProvidersRegistry.newProvider("stack_trace");
  public static final int PROVIDER_WALL_TIME_STACK_TRACE =
      ProvidersRegistry.newProvider("wall_time_stack_trace");
  public static final int PROVIDER_NATIVE_STACK_TRACE =
      ProvidersRegistry.newProvider("native_stack_trace");

  private static final String LOG_TAG = "StackFrameThread";
  private static final int ALL_THREADS = 0;

  @GuardedBy("this") private @Nullable Thread mProfilerThread;
  @GuardedBy("this") private final Context mContext;
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

  /**
   * Calculate all possible CPUProfiler tracers that correspond to the given providers bitset.
   */
  private static int providersToTracers(int providers) {
    int tracers = 0;
    if ((providers & (PROVIDER_STACK_FRAME | PROVIDER_WALL_TIME_STACK_TRACE)) != 0) {
      tracers |=
          CPUProfiler.TRACER_DALVIK
              | CPUProfiler.TRACER_ART_7_0
              | CPUProfiler.TRACER_ART_6_0
              | CPUProfiler.TRACER_ART_5_1;
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
  private synchronized boolean initProfiler() {
    try {
      return CPUProfiler.init(mContext);
    } catch (Exception ex) {
      Log.e(LOG_TAG, ex.getMessage(), ex);
      return false;
    }
  }

  private synchronized boolean enableInternal(int sampleRateMs, int enabledProviders) {
    if (!initProfiler()) {
      return false;
    }

    if (sampleRateMs <= 0) {
      sampleRateMs = CPUProfiler.DEFAULT_PROFILER_SAMPLING_RATE_MS;
    }

    // Default to get stack traces from all threads, override for wall time
    // stack profiling of main thread on-demand.
    int tids = ALL_THREADS;
    if ((enabledProviders & PROVIDER_WALL_TIME_STACK_TRACE) != 0) {
      tids = Process.myPid();
    } else {
      if (mSystemClockTimeIntervalMs == -1) {
        mSystemClockTimeIntervalMs = nativeSystemClockTickIntervalMs();
      }
      sampleRateMs = Math.max(sampleRateMs, mSystemClockTimeIntervalMs);
    }
    // For now, we'll just keep an eye on the main thread. Eventually we
    // might want to pass a list of all the interesting threads.
    // To use the setitimer logic, pass 0 as the second argument.
    boolean started =
        CPUProfiler.startProfiling(providersToTracers(enabledProviders), sampleRateMs, tids);
    if (!started) {
      return false;
    }

    Logger.writeEntryWithoutMatch(
        ProfiloConstants.PROVIDER_PROFILO_SYSTEM,
        EntryType.TRACE_ANNOTATION,
        Identifiers.CPU_SAMPLING_INTERVAL_MS,
        sampleRateMs);

    mEnabled = true;
    return mEnabled;
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
    mProfilerThread =
        new Thread(
            new Runnable() {
              public void run() {
                Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT);
                boolean enabled =
                    enableInternal(context.cpuSamplingRateMs, context.enabledProviders);
                if (!enabled) {
                  return;
                }
                mSavedTraceContext = context;
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

  @DoNotStrip
  private static native int nativeSystemClockTickIntervalMs();
}
