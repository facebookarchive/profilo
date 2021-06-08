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
package com.facebook.profilo.provider.systemcounters;

import android.annotation.SuppressLint;
import android.os.Debug;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.os.Process;
import com.facebook.jni.HybridData;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.core.TraceEvents;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.MultiBufferLogger;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.DoNotOptimize;
import com.facebook.soloader.SoLoader;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

/**
 * pulling the periodic timer into its own class so more counters can be added. the class itself is
 * not (currently) its own thread. Might get there eventually.
 */
@DoNotStrip
public final class SystemCounterThread extends BaseTraceProvider {

  public static interface CounterCollector {
    void log(MultiBufferLogger logger);
  }

  public static final int PROVIDER_SYSTEM_COUNTERS =
      ProvidersRegistry.newProvider("system_counters");
  public static final int PROVIDER_HIGH_FREQ_THREAD_COUNTERS =
      ProvidersRegistry.newProvider("high_freq_main_thread_counters");

  private static final int MSG_SYSTEM_COUNTERS = 1;
  private static final int MSG_HIGH_FREQ_THREAD_COUNTERS = 2;
  private static final int MSG_SYSTEM_COUNTERS_EXPENSIVE = 3;

  @DoNotStrip private HybridData mHybridData;

  private static final String LOG_TAG = "SystemCounterThread";

  public static final String SYSTEM_COUNTERS_SAMPLING_RATE_CONFIG_PARAM =
      "provider.system_counters.sampling_rate_ms";
  public static final String SYSTEM_COUNTERS_EXPENSIVE_SAMPLING_RATE_CONFIG_PARAM =
      "provider.system_counters.expensive_sampling_rate_ms";
  public static final String HIGH_FREQ_COUNTERS_SAMPLING_RATE_CONFIG_PARAM =
      "provider.high_freq_main_thread_counters.sampling_rate_ms";
  private static final int DEFAULT_COUNTER_PERIODIC_TIME_MS = 50;
  private static final int DEFAULT_COUNTER_EXPENSIVE_TIME_MS = 1000;
  private static final int DEFAULT_HIGH_FREQ_COUNTERS_PERIODIC_TIME_MS = 7;

  @GuardedBy("this")
  private boolean mEnabled;

  @GuardedBy("this")
  private HandlerThread mHandlerThread;

  @GuardedBy("this")
  private Handler mHandler;

  @GuardedBy("this")
  @Nullable
  private final CounterCollector mCounterCollector;

  @GuardedBy("this")
  private boolean mAllThreadsMode;

  @GuardedBy("this")
  private SystemCounterLogger mSystemCounterLogger;

  @GuardedBy("this")
  private volatile boolean mHighFrequencyMode;

  public SystemCounterThread() {
    this(null);
  }

  /**
   * @param periodicWork - a Runnable that will execute on the same cadence as the rest of the
   *     system counters.
   */
  public SystemCounterThread(@Nullable CounterCollector periodicWork) {
    super("profilo_systemcounters");
    mCounterCollector = periodicWork;
    mSystemCounterLogger = new SystemCounterLogger(getLogger());
  }

  private native HybridData initHybrid(MultiBufferLogger logger);

  native void logCounters();

  native void logExpensiveCounters();

  native void logHighFrequencyThreadCounters();

  native void logTraceAnnotations();

  native void nativeSetHighFrequencyMode(boolean enabled);

  public void setHighFrequencyMode(boolean enabled) {
    mHighFrequencyMode = enabled;
    nativeSetHighFrequencyMode(enabled);
  }

  @DoNotStrip
  static native void nativeAddToWhitelist(int targetThread);

  @DoNotStrip
  static native void nativeRemoveFromWhitelist(int targetThread);

  @SuppressLint({
    "BadMethodUse-android.os.HandlerThread._Constructor",
    "BadMethodUse-java.lang.Thread.start",
  })
  private synchronized void initHandler() {
    if (mHandler != null) {
      return;
    }

    mHandlerThread = new HandlerThread("Prflo:Counters");
    mHandlerThread.start();
    mHandler =
        new Handler(mHandlerThread.getLooper()) {
          @Override
          public void handleMessage(Message msg) {
            postThreadWork(msg.what, msg.arg1 /* delay */);
          }
        };
  }

  private synchronized boolean shouldRun() {
    return mEnabled;
  }

  synchronized void postThreadWork(int what, int delay) {
    if (!shouldRun()) {
      return;
    }

    switch (what) {
      case MSG_SYSTEM_COUNTERS:
        mSystemCounterLogger.logProcessCounters();
        logCounters();
        if (mCounterCollector != null) {
          mCounterCollector.log(getLogger());
        }
        break;
      case MSG_SYSTEM_COUNTERS_EXPENSIVE:
        logExpensiveCounters();
        break;
      case MSG_HIGH_FREQ_THREAD_COUNTERS:
        logHighFrequencyThreadCounters();
        break;
      default:
        throw new IllegalArgumentException("Unknown message type");
    }

    Message nextMessage = mHandler.obtainMessage(what, delay, 0);
    mHandler.sendMessageDelayed(nextMessage, delay);
  }

  @Override
  protected synchronized void enable() {
    mHybridData = initHybrid(getLogger());
    mEnabled = true;
    initHandler();
    final TraceContext traceContext = getEnablingTraceContext();
    if (TraceEvents.isEnabled(PROVIDER_SYSTEM_COUNTERS)) {
      setHighFrequencyMode(false);
      mAllThreadsMode = true;
      Debug.startAllocCounting();
      mSystemCounterLogger.reset();
      int samplingRateMs =
          traceContext == null
              ? DEFAULT_COUNTER_PERIODIC_TIME_MS
              : traceContext.mTraceConfigExtras.getIntParam(
                  SYSTEM_COUNTERS_SAMPLING_RATE_CONFIG_PARAM, DEFAULT_COUNTER_PERIODIC_TIME_MS);
      mHandler.obtainMessage(MSG_SYSTEM_COUNTERS, samplingRateMs, 0).sendToTarget();

      int expensiveSamplingRateMs =
          traceContext == null
              ? DEFAULT_COUNTER_EXPENSIVE_TIME_MS
              : traceContext.mTraceConfigExtras.getIntParam(
                  SYSTEM_COUNTERS_EXPENSIVE_SAMPLING_RATE_CONFIG_PARAM,
                  DEFAULT_COUNTER_EXPENSIVE_TIME_MS);
      mHandler
          .obtainMessage(MSG_SYSTEM_COUNTERS_EXPENSIVE, expensiveSamplingRateMs, 0)
          .sendToTarget();
    }
    if (TraceEvents.isEnabled(PROVIDER_HIGH_FREQ_THREAD_COUNTERS)) {
      // Add Main Thread to the whitelist
      WhitelistApi.add(Process.myPid());
      setHighFrequencyMode(true);
      int samplingRateMs =
          traceContext == null
              ? DEFAULT_HIGH_FREQ_COUNTERS_PERIODIC_TIME_MS
              : traceContext.mTraceConfigExtras.getIntParam(
                  HIGH_FREQ_COUNTERS_SAMPLING_RATE_CONFIG_PARAM,
                  DEFAULT_HIGH_FREQ_COUNTERS_PERIODIC_TIME_MS);
      mHandler.obtainMessage(MSG_HIGH_FREQ_THREAD_COUNTERS, samplingRateMs, 0).sendToTarget();
    }
  }

  @Override
  protected synchronized void disable() {
    if (mEnabled) {
      // inject one last time before shutting down.
      mSystemCounterLogger.logProcessCounters();
      if (mAllThreadsMode) {
        logCounters();
        logExpensiveCounters();
      }
      if (mHighFrequencyMode) {
        logHighFrequencyThreadCounters();
        logTraceAnnotations();
      }
    }
    mEnabled = false;
    mAllThreadsMode = false;
    setHighFrequencyMode(false);
    if (mHybridData != null) {
      mHybridData.resetNative();
      mHybridData = null;
    }

    if (mHandlerThread != null) {
      mHandlerThread.quit();
      mHandlerThread = null;
    }
    mHandler = null;
    Debug.stopAllocCounting();
  }

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_SYSTEM_COUNTERS | PROVIDER_HIGH_FREQ_THREAD_COUNTERS;
  }

  @Override
  protected int getTracingProviders() {
    if (!mEnabled) {
      return 0;
    }
    int tracingProviders = 0;
    if (mAllThreadsMode) {
      tracingProviders |= PROVIDER_SYSTEM_COUNTERS;
    }
    if (mHighFrequencyMode) {
      tracingProviders |= PROVIDER_HIGH_FREQ_THREAD_COUNTERS;
    }
    return tracingProviders;
  }

  @DoNotOptimize
  public static class WhitelistApi {
    // Allow lazy loading of native lib whenever any Whitelist
    // API (add/remove) is called
    static {
      SoLoader.loadLibrary("profilo_systemcounters");
    }

    @DoNotOptimize
    public static void add(int threadId) {
      nativeAddToWhitelist(threadId);
    }

    @DoNotOptimize
    public static void remove(int threadId) {
      nativeRemoveFromWhitelist(threadId);
    }
  }
}
