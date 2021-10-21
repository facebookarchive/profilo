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

public interface TraceOrchestratorListener
    extends TraceWriterListener, BackgroundUploadService.BackgroundUploadListener {

  void onTraceStart(TraceContext context);

  void onTraceStop(TraceContext context);

  void onTraceAbort(TraceContext context);

  boolean canUploadFlushedTrace(TraceContext trace, File traceFile);

  void onTraceScheduledForUpload(TraceContext trace);

  void onTraceFlushedDoFileAnalytics(
      int totalErrors, int trimmedFromCount, int trimmedFromAge, int filesAddedToUpload);
  /** Config update has been received but not yet applied. */
  void onNewConfigAvailable();
  /** New updated config has been applied. */
  void onConfigUpdated();

  void onProvidersInitialized(TraceContext traceContext);

  void onProvidersStop(TraceContext traceContext, int activeProviders);
}
