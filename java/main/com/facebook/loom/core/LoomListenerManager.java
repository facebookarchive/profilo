// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import com.facebook.loom.ipc.TraceContext;
import java.io.File;
import java.util.Iterator;
import java.util.concurrent.CopyOnWriteArrayList;

/* package */ class LoomListenerManager implements TraceOrchestrator.LoomListener {

  private final CopyOnWriteArrayList<TraceOrchestrator.LoomListener> mEventListeners =
      new CopyOnWriteArrayList<>();

  @Override
  public void onTraceStart(TraceContext context) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceStart(context);
    }
  }

  @Override
  public void onTraceStop(TraceContext context) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceStop(context);
    }
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceAbort(context);
    }
  }

  @Override
  public void onUploadSuccessful(File file) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onUploadSuccessful(file);
    }
  }

  @Override
  public void onUploadFailed(File file) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onUploadFailed(file);
    }
  }

  @Override
  public void onTraceFlushed(File trace) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceFlushed(trace);
    }
  }

  @Override
  public void onTraceFlushedDoFileAnalytics(
      int totalErrors, int trimmedFromCount, int trimmedFromAge, int filesAddedToUpload) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceFlushedDoFileAnalytics(
          totalErrors,
          trimmedFromCount,
          trimmedFromAge,
          filesAddedToUpload);
    }
  }

  @Override
  public void onConfigChanged() {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onConfigChanged();
    }
  }

  @Override
  public void onTraceWriteStart(long traceId, int flags, String file) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteStart(traceId, flags, file);
    }
  }

  @Override
  public void onTraceWriteEnd(long traceId, int crc) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteEnd(traceId, crc);
    }
  }

  @Override
  public void onTraceWriteAbort(long traceId, int abortReason) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onTraceWriteAbort(traceId, abortReason);
    }
  }

  @Override
  public void onLoggerException(Throwable t) {
    Iterator<TraceOrchestrator.LoomListener> iterator = getIterator();
    while (iterator.hasNext()) {
      iterator.next().onLoggerException(t);
    }
  }

  public void addEventListener(TraceOrchestrator.LoomListener listener) {
    mEventListeners.add(listener);
  }

  public void removeEventListener(TraceOrchestrator.LoomListener listener) {
    mEventListeners.remove(listener);
  }

  private Iterator<TraceOrchestrator.LoomListener> getIterator() {
    return mEventListeners.iterator();
  }
}
