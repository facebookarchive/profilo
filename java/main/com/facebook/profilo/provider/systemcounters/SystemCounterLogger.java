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

import android.os.Build;
import android.os.Debug;
import com.facebook.profilo.core.Identifiers;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.MultiBufferLogger;
import javax.annotation.concurrent.NotThreadSafe;

@NotThreadSafe
public class SystemCounterLogger {

  private static final String GC_COUNT_RUNTIME_STAT = "art.gc.gc-count";
  private static final String GC_TIME_RUNTIME_STAT = "art.gc.gc-time";
  private static final String GC_BLOCKING_COUNT_RUNTIME_STAT = "art.gc.blocking-gc-count";
  private static final String GC_BLOCKING_TIME_RUNTIME_STAT = "art.gc.blocking-gc-time";
  private final MultiBufferLogger mLogger;

  // Allocations
  private long mAllocSize;
  private long mAllocCount;
  // GC
  private long mGcCount;
  private long mGcTime;
  private long mBlockingGcCount;
  private long mBlockingGcTime;
  // Heap
  private long mJavaMax;
  private long mJavaFree;
  private long mJavaUsed;
  private long mJavaTotal;

  SystemCounterLogger(MultiBufferLogger logger) {
    mLogger = logger;
  }

  public void logProcessCounters() {
    // Counters from android.os.Debug
    // - Allocations
    // Even, though function android.os.Debug#getGlobalAllocCount is deprecated
    // it's still working, so we can use it.
    long allocCount = Debug.getGlobalAllocCount();
    logMonotonicProcessCounter(Identifiers.GLOBAL_ALLOC_COUNT, allocCount, mAllocCount);
    mAllocCount = allocCount;
    // Even, though function android.os.Debug#getGlobalAllocSize is deprecated
    // it's still working, so we can use it.
    long allocSize = Debug.getGlobalAllocSize();
    logMonotonicProcessCounter(Identifiers.GLOBAL_ALLOC_SIZE, allocSize, mAllocSize);
    mAllocSize = allocSize;
    // - Garbage Collector stats (Android 6+ / API 23 only)
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      String gcCountStr = Debug.getRuntimeStat(GC_COUNT_RUNTIME_STAT);
      if (gcCountStr != null) {
        long gcCount = Long.parseLong(gcCountStr);
        logMonotonicProcessCounter(Identifiers.GLOBAL_GC_INVOCATION_COUNT, gcCount, mGcCount);
        mGcCount = gcCount;
      }
      String gcTimeStr = Debug.getRuntimeStat(GC_TIME_RUNTIME_STAT);
      if (gcTimeStr != null) {
        long gcTime = Long.parseLong(gcTimeStr);
        logMonotonicProcessCounter(Identifiers.GLOBAL_GC_TIME, gcTime, mGcTime);
        mGcTime = gcTime;
      }
      String blockingGcCountStr = Debug.getRuntimeStat(GC_BLOCKING_COUNT_RUNTIME_STAT);
      if (blockingGcCountStr != null) {
        long blockingGcCount = Long.parseLong(blockingGcCountStr);
        logMonotonicProcessCounter(
            Identifiers.GLOBAL_BLOCKING_GC_COUNT, blockingGcCount, mBlockingGcCount);
        mBlockingGcCount = blockingGcCount;
      }
      String blockingGcTimeStr = Debug.getRuntimeStat(GC_BLOCKING_TIME_RUNTIME_STAT);
      if (blockingGcTimeStr != null) {
        long blockingGcTime = Long.parseLong(blockingGcTimeStr);
        logMonotonicProcessCounter(
            Identifiers.GLOBAL_BLOCKING_GC_TIME, blockingGcTime, mBlockingGcTime);
        mBlockingGcTime = blockingGcTime;
      }
    }
    // Counters from runtime
    Runtime runtime = Runtime.getRuntime();
    // max memory java can request
    // totalMemory = total mem java requested
    // freeMemory = free memory from total memory
    long javaMax = runtime.maxMemory();
    long javaTotal = runtime.totalMemory();
    long javaUsed = javaTotal - runtime.freeMemory();
    long javaFree = javaMax - javaUsed; // We count unrequested memory as "free"
    logNonMonotonicProcessCounter(Identifiers.JAVA_ALLOC_BYTES, javaUsed, mJavaUsed);
    logNonMonotonicProcessCounter(Identifiers.JAVA_FREE_BYTES, javaFree, mJavaFree);
    logNonMonotonicProcessCounter(Identifiers.JAVA_MAX_BYTES, javaMax, mJavaMax);
    logNonMonotonicProcessCounter(Identifiers.JAVA_TOTAL_BYTES, javaTotal, mJavaTotal);
    mJavaMax = javaMax;
    mJavaTotal = javaTotal;
    mJavaUsed = javaUsed;
    mJavaFree = javaFree;
  }

  /**
   * Logs the actual counter value when it moves. If a value doesn't change between the samples then
   * it's ignored.
   */
  private void logMonotonicProcessCounter(int key, long value, long prevValue) {
    if (value <= prevValue) {
      return;
    }
    logProcessCounter(key, value);
  }

  private void logNonMonotonicProcessCounter(int key, long value, long prevValue) {
    if (value == prevValue) {
      return;
    }
    logProcessCounter(key, value);
  }

  private void logProcessCounter(int key, long value) {
    mLogger.writeStandardEntry(
        Logger.FILL_TIMESTAMP | Logger.FILL_TID,
        EntryType.COUNTER,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        key,
        ProfiloConstants.NONE,
        value);
  }

  public void reset() {
    mAllocCount = 0;
    mAllocSize = 0;
    mGcCount = 0;
    mGcTime = 0;
    mBlockingGcCount = 0;
    mBlockingGcTime = 0;
    mJavaFree = 0;
    mJavaMax = 0;
    mJavaTotal = 0;
    mJavaUsed = 0;
  }
}
