package com.facebook.profilo.ipc;

import com.facebook.profilo.ipc.ILoomMultiProcessTraceService;
import com.facebook.profilo.ipc.TraceConfigData;
import com.facebook.profilo.ipc.TraceContext;

interface ILoomMultiProcessTraceListener {

  /**
   * Called after the ILoomMultiProcessTraceService from another
   * process has received the listener
   */
  void onReceive(ILoomMultiProcessTraceService service);

  void onTraceStart(in TraceContext context);

  void onTraceStop(in TraceContext context);

  void onTraceAbort(in TraceContext context);

  void waitForTraceClose(in long traceId);

  void onConfigChanged(in TraceConfigData configData);

}
