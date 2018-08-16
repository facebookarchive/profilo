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

import android.os.Process;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

public class StackTraceWhitelist {
  static {
    SoLoader.loadLibrary("profilo_stacktrace");
  }

  public static void add(int threadId) {
    nativeAddToWhitelist(threadId);
  }

  public static void remove(int threadId) {
    if (threadId != Process.myPid()) {
      // When in wall clock mode, we always profile the main thread, so we don't
      // support de-whitelisting it.
      nativeRemoveFromWhitelist(threadId);
    }
  }

  @DoNotStrip
  private static native void nativeAddToWhitelist(int targetThread);

  @DoNotStrip
  private static native void nativeRemoveFromWhitelist(int targetThread);
}
