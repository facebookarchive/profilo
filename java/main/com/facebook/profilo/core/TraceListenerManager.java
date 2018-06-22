/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.facebook.profilo.core;

import com.facebook.profilo.ipc.TraceContext;
import java.io.File;
import java.util.Iterator;
import java.util.concurrent.CopyOnWriteArrayList;

/* package */ class TraceListenerManager implements TraceOrchestrator.TraceListener {

  private final CopyOnWriteArrayList<TraceOrchestrator.TraceListener> mEventListeners =
      new CopyOnWriteArrayList<>();

  @Override
  public void onTraceStart(TraceContext context) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceStart(context);
    }
  }

  @Override
  public void onTraceStop(TraceContext context) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceStop(context);
    }
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceAbort(context);
    }
  }

  @Override
  public void onUploadSuccessful(File file) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onUploadSuccessful(file);
    }
  }

  @Override
  public void onUploadFailed(File file) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onUploadFailed(file);
    }
  }

  @Override
  public void onTraceFlushed(File trace, long traceId) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceFlushed(trace, traceId);
    }
  }

  @Override
  public void onTraceFlushedDoFileAnalytics(
      int totalErrors, int trimmedFromCount, int trimmedFromAge, int filesAddedToUpload) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceFlushedDoFileAnalytics(
          totalErrors,
          trimmedFromCount,
          trimmedFromAge,
          filesAddedToUpload);
    }
  }

  @Override
  public void onBeforeConfigUpdate() {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onBeforeConfigUpdate();
    }
  }

  @Override
  public void onAfterConfigUpdate() {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onAfterConfigUpdate();
    }
  }

  @Override
  public void onTraceWriteStart(long traceId, int flags, String file) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteStart(traceId, flags, file);
    }
  }

  @Override
  public void onTraceWriteEnd(long traceId, int crc) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteEnd(traceId, crc);
    }
  }

  @Override
  public void onTraceWriteAbort(long traceId, int abortReason) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteAbort(traceId, abortReason);
    }
  }

  @Override
  public void onLoggerException(Throwable t) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onLoggerException(t);
    }
  }

  @Override
  public void onProvidersInitialized(TraceContext ctx) {
    Iterator<TraceOrchestrator.TraceListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onProvidersInitialized(ctx);
    }
  }

  public void addEventListener(TraceOrchestrator.TraceListener listener) {
    mEventListeners.add(listener);
  }

  public void removeEventListener(TraceOrchestrator.TraceListener listener) {
    mEventListeners.remove(listener);
  }

  private Iterator<TraceOrchestrator.TraceListener> getIterator() {
    return mEventListeners.iterator();
  }
}
