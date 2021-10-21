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
import java.util.Iterator;
import java.util.concurrent.CopyOnWriteArrayList;

/* package */ class TraceListenerManager implements TraceOrchestratorListener {

  private final CopyOnWriteArrayList<TraceOrchestratorListener> mEventListeners =
      new CopyOnWriteArrayList<>();

  @Override
  public void onTraceStart(TraceContext context) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceStart(context);
    }
  }

  @Override
  public void onTraceStop(TraceContext context) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceStop(context);
    }
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceAbort(context);
    }
  }

  @Override
  public void onUploadSuccessful(File file) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onUploadSuccessful(file);
    }
  }

  @Override
  public void onUploadFailed(File file, int reason) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onUploadFailed(file, reason);
    }
  }

  @Override
  public void onTraceScheduledForUpload(TraceContext trace) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceScheduledForUpload(trace);
    }
  }

  @Override
  public boolean canUploadFlushedTrace(TraceContext trace, File traceFile) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    // Upload will return false if ANY listener denies upload
    while (iterator.hasNext()) {
      if (!iterator.next().canUploadFlushedTrace(trace, traceFile)) {
        return false;
      }
    }
    return true;
  }

  @Override
  public void onTraceFlushedDoFileAnalytics(
      int totalErrors, int trimmedFromCount, int trimmedFromAge, int filesAddedToUpload) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator
          .next()
          .onTraceFlushedDoFileAnalytics(
              totalErrors, trimmedFromCount, trimmedFromAge, filesAddedToUpload);
    }
  }

  @Override
  public void onNewConfigAvailable() {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onNewConfigAvailable();
    }
  }

  @Override
  public void onConfigUpdated() {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onConfigUpdated();
    }
  }

  @Override
  public void onTraceWriteStart(TraceContext trace) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteStart(trace);
    }
  }

  @Override
  public void onTraceWriteEnd(TraceContext trace) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteEnd(trace);
    }
  }

  @Override
  public void onTraceWriteAbort(TraceContext trace, int abortReason) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteAbort(trace, abortReason);
    }
  }

  @Override
  public void onTraceWriteException(TraceContext trace, Throwable t) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteException(trace, t);
    }
  }

  @Override
  public void onProvidersInitialized(TraceContext traceContext) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onProvidersInitialized(traceContext);
    }
  }

  @Override
  public void onProvidersStop(TraceContext traceContext, int activeProviders) {
    Iterator<TraceOrchestratorListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onProvidersStop(traceContext, activeProviders);
    }
  }

  public void addEventListener(TraceOrchestratorListener listener) {
    mEventListeners.add(listener);
  }

  public void removeEventListener(TraceOrchestratorListener listener) {
    mEventListeners.remove(listener);
  }

  private Iterator<TraceOrchestratorListener> getIterator() {
    return mEventListeners.iterator();
  }
}
