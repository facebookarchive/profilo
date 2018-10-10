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
package com.facebook.profilo.provider.systemcounters.api;

import android.os.Process;
import com.facebook.profilo.provider.systemcounters.SystemCounterThread;

/**
 * <pre>When in high_freq_main_thread_counters mode tracing, typically only the main thread is
 * traced due to the required overhead. However, this API can be used to optionally whitelist
 * threads so that they also appear in high_freq_main_thread_counters provider enabled traces.
 *
 * Typical usage (from a worker thread):
 *
 * SystemCountersWhitelistApi.add()         //enables calling thread for profiling
 * SystemCountersWhitelistApi.add(<tid_1>)  //happends by Java thread that wants to trace a codeblock
 * Start profiling
 *    // Do stuff i.e. code block under trace
 * Stop profiling
 * SystemCountersWhitelistApi.remove()      // Remove calling thread id from whitelist of threads
 **/
public class SystemCountersWhitelistApi {
  /** Only Profilo is supposed to set this to true. */
  private static volatile boolean sHasProfilo = false;

  public static boolean hasProfilo() {
    return sHasProfilo;
  }

  public static void enableProfilo() {
    sHasProfilo = true;
  }

  public static void add() {
    add(Process.myTid());
  }

  public static void add(int threadId) {
    if (sHasProfilo) {
      SystemCounterThread.WhitelistApi.add(threadId);
    }
  }

  public static void remove() {
    remove(Process.myTid());
  }

  public static void remove(int threadId) {
    if (sHasProfilo) {
      SystemCounterThread.WhitelistApi.remove(threadId);
    }
  }
}
