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
package com.facebook.profilo.core;

import android.os.HandlerThread;
import android.os.Looper;
import javax.annotation.Nullable;

public class TraceControlThreadHolder {

  @Nullable private static TraceControlThreadHolder sInstance;
  @Nullable private HandlerThread mHandlerThread;

  private TraceControlThreadHolder() {}

  private synchronized HandlerThread ensureThreadInitialized() {
    if (mHandlerThread == null) {
      mHandlerThread = new HandlerThread("Prflo:TraceCtl");
      mHandlerThread.start();
    }
    return mHandlerThread;
  }

  public Looper getLooper() {
    return ensureThreadInitialized().getLooper();
  }

  public static synchronized TraceControlThreadHolder getInstance() {
    if (sInstance == null) {
      sInstance = new TraceControlThreadHolder();
    }
    return sInstance;
  }
}
