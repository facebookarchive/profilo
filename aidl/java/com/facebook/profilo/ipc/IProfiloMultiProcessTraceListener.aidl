package com.facebook.profilo.ipc;

import com.facebook.profilo.ipc.IProfiloMultiProcessTraceService;
import com.facebook.profilo.ipc.TraceConfigData;
import com.facebook.profilo.ipc.TraceContext;

interface IProfiloMultiProcessTraceListener {

  /**
   * Called after the IProfiloMultiProcessTraceService from another
   * process has received the listener
   */
  void onReceive(IProfiloMultiProcessTraceService service);

  void onTraceStart(in TraceContext context);

  void onTraceStop(in TraceContext context);

  void onTraceAbort(in TraceContext context);

  void waitForTraceClose(in long traceId);

  void onConfigChanged(in TraceConfigData configData);

}
