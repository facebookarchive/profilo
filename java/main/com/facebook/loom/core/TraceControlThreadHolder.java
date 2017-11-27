// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import android.os.HandlerThread;
import android.os.Looper;
import javax.annotation.Nullable;

public class TraceControlThreadHolder {

  @Nullable private static TraceControlThreadHolder sInstance;
  @Nullable private HandlerThread mHandlerThread;

  private TraceControlThreadHolder() {}

  private synchronized HandlerThread ensureThreadInitialized() {
    if (mHandlerThread == null) {
      mHandlerThread = new HandlerThread("Loom/TraceCtrl");
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
