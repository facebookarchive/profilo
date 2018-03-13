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

import android.os.Debug;
import com.facebook.profilo.core.Identifiers;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.logger.Logger;

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
