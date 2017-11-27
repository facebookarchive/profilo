// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.writer;

import com.facebook.proguard.annotations.DoNotStrip;

@DoNotStrip
public interface NativeTraceWriterCallbacks {

  @DoNotStrip
  void onTraceWriteStart(long traceId, int flags, String file);

  @DoNotStrip
  void onTraceWriteEnd(long traceId);

  @DoNotStrip
  void onTraceWriteAbort(long traceId, int abortReason);
}
