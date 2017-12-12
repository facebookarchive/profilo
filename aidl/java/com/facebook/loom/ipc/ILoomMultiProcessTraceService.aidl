package com.facebook.loom.ipc;

import com.facebook.loom.ipc.ILoomMultiProcessTraceListener;

interface ILoomMultiProcessTraceService {

  /**
   * Registers a listener that will receive notifications
   * for future trace events
   */
  void registerListener(ILoomMultiProcessTraceListener listener);

  /** 
   * Notifies the service that a trace this listener has started has
   * been aborted.
   */
  void onTraceAbortedInSecondary(in long traceId, in int abortReason);
}
