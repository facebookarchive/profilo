// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.profilo.core;

import com.facebook.profilo.ipc.TraceContext;

public interface TraceWriterListener {
  void onTraceWriteStart(TraceContext trace);

  void onTraceWriteEnd(TraceContext trace);

  void onTraceWriteAbort(TraceContext trace, int abortReason);

  void onTraceWriteException(TraceContext trace, Throwable t);
}
