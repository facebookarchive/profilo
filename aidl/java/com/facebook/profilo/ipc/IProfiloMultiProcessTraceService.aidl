package com.facebook.profilo.ipc;

import com.facebook.profilo.ipc.IProfiloMultiProcessTraceListener;

interface IProfiloMultiProcessTraceService {

  /**
   * Registers a listener that will receive notifications
   * for future trace events
   */
  void registerListener(IProfiloMultiProcessTraceListener listener);

  /**
   * Notifies the service that a trace this listener has started has
   * been aborted.
   */
  void onTraceAbortedInSecondary(in long traceId, in int abortReason);
}
