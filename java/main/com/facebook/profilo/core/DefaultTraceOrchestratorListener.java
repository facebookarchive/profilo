// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.core;

import com.facebook.profilo.ipc.TraceContext;
import java.io.File;

public abstract class DefaultTraceOrchestratorListener implements TraceOrchestrator.TraceListener {
  @Override
  public void onTraceFlushed(File trace, long traceId) {}

  @Override
  public void onTraceFlushedDoFileAnalytics(
      int totalErrors, int trimmedFromCount, int trimmedFromAge, int filesAddedToUpload) {}

  @Override
  public void onBeforeConfigUpdate() {}

  @Override
  public void onAfterConfigUpdate() {}

  @Override
  public void onProvidersInitialized(TraceContext ctx) {}

  @Override
  public void onUploadSuccessful(File file) {}

  @Override
  public void onUploadFailed(File file) {}

  @Override
  public void onTraceStart(TraceContext context) {}

  @Override
  public void onTraceStop(TraceContext context) {}

  @Override
  public void onTraceAbort(TraceContext context) {}

  @Override
  public void onLoggerException(Throwable t) {}

  @Override
  public void onTraceWriteStart(long traceId, int flags, String file) {}

  @Override
  public void onTraceWriteEnd(long traceId, int crc) {}

  @Override
  public void onTraceWriteAbort(long traceId, int abortReason) {}
}
