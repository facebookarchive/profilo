// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.stacktrace;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.Context;
import android.os.Process;
import android.util.Log;
import com.facebook.loom.core.Identifiers;
import com.facebook.loom.core.LoomConstants;
import com.facebook.loom.core.ProvidersRegistry;
import com.facebook.loom.core.TraceOrchestrator;
import com.facebook.loom.entries.EntryType;
import com.facebook.loom.ipc.TraceContext;
import com.facebook.loom.logger.Logger;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

public final class StackFrameThread implements TraceOrchestrator.TraceProvider {

  static {
    SoLoader.loadLibrary("profilo_stacktrace");
  }

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
        LoomConstants.PROVIDER_LOOM_SYSTEM,
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
  public synchronized void onEnable(final TraceContext context, File extraDataFolder) {
    if (mSavedTraceContext != null) {
      return;
    }
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
            "Loom:Profiler");
    mProfilerThread.start();
  }

  @Override
  public synchronized void onDisable(TraceContext context, File extraDataFolder) {
    if (!mEnabled) {
      mProfilerThread = null;
      return;
    }
    if (context != mSavedTraceContext) {
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

  @DoNotStrip
  private static native int nativeSystemClockTickIntervalMs();
}
