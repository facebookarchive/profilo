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

import com.facebook.profilo.ipc.TraceContext;
import java.io.File;

public abstract class DefaultTraceOrchestratorListener implements TraceOrchestratorListener {

  @Override
  public boolean canUploadFlushedTrace(TraceContext trace, File traceFile) {
    return true;
  }

  @Override
  public void onTraceScheduledForUpload(TraceContext trace) {}

  @Override
  public void onTraceFlushedDoFileAnalytics(
      int totalErrors, int trimmedFromCount, int trimmedFromAge, int filesAddedToUpload) {}

  @Override
  public void onNewConfigAvailable() {}

  @Override
  public void onConfigUpdated() {}

  @Override
  public void onProvidersInitialized(TraceContext traceContext) {}

  @Override
  public void onProvidersStop(TraceContext traceContext, int activeProviders) {}

  @Override
  public void onUploadSuccessful(File file) {}

  @Override
  public void onUploadFailed(File file, int reason) {}

  @Override
  public void onTraceStart(TraceContext context) {}

  @Override
  public void onTraceStop(TraceContext context) {}

  @Override
  public void onTraceAbort(TraceContext context) {}

  @Override
  public void onTraceWriteException(TraceContext traceContext, Throwable t) {}

  @Override
  public void onTraceWriteStart(TraceContext traceContext) {}

  @Override
  public void onTraceWriteEnd(TraceContext traceContext) {}

  @Override
  public void onTraceWriteAbort(TraceContext traceContext, int abortReason) {}
}
