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

package com.facebook.profilo.provider.systemcounters;

import android.annotation.SuppressLint;
import android.os.Debug;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import com.facebook.jni.HybridData;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.core.TraceEvents;
import com.facebook.proguard.annotations.DoNotStrip;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

/**
 * pulling the periodic timer into its own class so more counters can be added. the class itself is
 * not (currently) its own thread. Might get there eventually.
 */
@DoNotStrip
public final class SystemCounterThread extends BaseTraceProvider {

  public static final int PROVIDER_SYSTEM_COUNTERS =
      ProvidersRegistry.newProvider("system_counters");
  public static final int PROVIDER_HIGH_FREQ_MAIN_THREAD_COUNTERS =
      ProvidersRegistry.newProvider("high_freq_main_thread_counters");

  private static final int MSG_SYSTEM_COUNTERS = 1;
  private static final int MSG_MAIN_THREAD_COUNTERS = 2;

  @DoNotStrip
  private HybridData mHybridData;

  private static final String LOG_TAG = "SystemCounterThread";

  private static final int DEFAULT_COUNTER_PERIODIC_TIME_MS = 50;
  private static final int MAIN_THREAD_COUNTER_PERIODIC_TIME_MS = 7;

  @GuardedBy("this") private boolean mEnabled;
  @GuardedBy("this") private HandlerThread mHandlerThread;
  @GuardedBy("this") private Handler mHandler;
  @GuardedBy("this") @Nullable private final Runnable mExtraRunnable;
  @GuardedBy("this") private boolean mAllThreadsMode;
  @GuardedBy("this") private int mHighFrequencyTid;

  public SystemCounterThread() {
    this(null);
  }

  /**
   * @param periodicWork - a Runnable that will execute on the same cadence as
   * the rest of the system counters.
   */
  public SystemCounterThread(@Nullable Runnable periodicWork) {
    super("profilo_systemcounters");
    mExtraRunnable = periodicWork;
  }

  private static native HybridData initHybrid();

  native void logCounters();
  native void logThreadCounters(int tid);
  native void logTraceAnnotations(int tid);

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
            postThreadWork(msg.what, msg.arg1 /* delay */, msg.arg2 /* tid */);
          }
        };
  }

  private synchronized boolean shouldRun() {
    return mEnabled;
  }

  synchronized void postThreadWork(int what, int delay, int tid) {
    if (!shouldRun()) {
      return;
    }

    switch (what) {
      case MSG_SYSTEM_COUNTERS:
        SystemCounterLogger.logSystemCounters();
        logCounters();
        if (mExtraRunnable != null) {
          mExtraRunnable.run();
        }
        break;
      case MSG_MAIN_THREAD_COUNTERS:
        logThreadCounters(tid);
        break;
      default:
        throw new IllegalArgumentException("Unknown message type");
    }

    Message nextMessage = mHandler.obtainMessage(what, delay, tid);
    mHandler.sendMessageDelayed(nextMessage, delay);
  }

  @Override
  protected synchronized void enable() {
    mHybridData = initHybrid();
    mEnabled = true;
    initHandler();
    if (TraceEvents.isEnabled(PROVIDER_SYSTEM_COUNTERS)) {
      mAllThreadsMode = true;
      Debug.startAllocCounting();
      mHandler
          .obtainMessage(MSG_SYSTEM_COUNTERS, DEFAULT_COUNTER_PERIODIC_TIME_MS, -1)
          .sendToTarget();
    }
    if (TraceEvents.isEnabled(PROVIDER_HIGH_FREQ_MAIN_THREAD_COUNTERS)) {
      int mainTid = android.os.Process.myPid();
      mHighFrequencyTid = mainTid;
      mHandler
          .obtainMessage(MSG_MAIN_THREAD_COUNTERS, MAIN_THREAD_COUNTER_PERIODIC_TIME_MS, mainTid)
          .sendToTarget();
    }
  }

  @Override
  protected synchronized void disable() {
    if (mEnabled) {
      // inject one last time before shutting down.
      SystemCounterLogger.logSystemCounters();
      if (mAllThreadsMode) {
        logCounters();
      }
      if (mHighFrequencyTid > 0) {
        logThreadCounters(mHighFrequencyTid);
        logTraceAnnotations(mHighFrequencyTid);
      }
    }
    mEnabled = false;
    mAllThreadsMode = false;
    mHighFrequencyTid = 0;
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
    return PROVIDER_SYSTEM_COUNTERS | PROVIDER_HIGH_FREQ_MAIN_THREAD_COUNTERS;
  }
}
