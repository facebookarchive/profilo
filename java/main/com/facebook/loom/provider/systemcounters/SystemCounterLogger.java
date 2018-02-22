// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.systemcounters;

import android.os.Debug;
import com.facebook.loom.core.Identifiers;
import com.facebook.loom.entries.EntryType;
import com.facebook.loom.logger.Logger;

public class SystemCounterLogger {


  public static void logSystemCounters() {
    logProcessCounters();
  }

  private static void logProcessCounters() {
    // Counters from android.os.Debug
    logProcessCounter(Identifiers.GLOBAL_ALLOC_COUNT, Debug.getGlobalAllocCount());
    logProcessCounter(Identifiers.GLOBAL_ALLOC_SIZE, Debug.getGlobalAllocSize());
    logProcessCounter(Identifiers.GLOBAL_GC_INVOCATION_COUNT, Debug.getGlobalGcInvocationCount());
    // Counters from runtime
    Runtime runtime = Runtime.getRuntime();
    // max memory java can request
    // totalMemory = total mem java requested
    // freeMemory = free memory from total memory
    long javaMax = runtime.maxMemory();
    long javaTotal = runtime.totalMemory();
    long javaUsed = javaTotal - runtime.freeMemory();
    long javaFree = javaMax - javaUsed; // We count unrequested memory as "free"
    logProcessCounter(Identifiers.JAVA_ALLOC_BYTES, javaUsed);
    logProcessCounter(Identifiers.JAVA_FREE_BYTES, javaFree);
    logProcessCounter(Identifiers.JAVA_MAX_BYTES, javaMax);
    logProcessCounter(Identifiers.JAVA_TOTAL_BYTES, javaTotal);
  }

  private static void logProcessCounter(int key, long value) {
    Logger.writeEntryWithoutMatch(
        SystemCounterThread.PROVIDER_SYSTEM_COUNTERS, EntryType.COUNTER, key, value);
  }
}
