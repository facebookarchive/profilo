// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.writer;

import com.facebook.jni.HybridData;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

@DoNotStrip
public final class NativeTraceWriter {
  static {
    SoLoader.loadLibrary("profilo");
  }

  @DoNotStrip
  private HybridData mHybridData;

  public NativeTraceWriter(
      String traceFolder,
      String tracePrefix,
      int preferredBufferSize,
      NativeTraceWriterCallbacks callbacks) {
    mHybridData = initHybrid(
      traceFolder,
      tracePrefix,
      preferredBufferSize,
      callbacks);
  }

  private static native HybridData initHybrid(
      String traceFolder,
      String tracePrefix,
      int preferredBufferSize,
      NativeTraceWriterCallbacks callbacks);

  public native void loop();
  public native String getTraceFolder(long traceID);
}
