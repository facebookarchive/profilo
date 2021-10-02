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

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.BufferLogger;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.LoggerWorkerThread;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.writer.NativeTraceWriterCallbacks;
import java.io.IOException;
import java.util.HashSet;
import java.util.Random;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

/**
 * Thread responsible for stopping the trace, which will synchronously gather contextual data for
 * the trace.
 */
@SuppressLint({
  "BadMethodUse-android.util.Log.d",
})
public class TraceControlHandler extends Handler {

  private static class AnnotationContext {
    public TraceContext mTraceContext;
    public String mAnnotation;

    public AnnotationContext(TraceContext context, String annotation) {
      mTraceContext = context;
      mAnnotation = annotation;
    }
  }

  private static final String LOG_TAG = "Profilo/TraceControlThread";
  private static final int MSG_TIMEOUT_TRACE = 0;
  private static final int MSG_START_TRACE = 1;
  private static final int MSG_STOP_TRACE = 2;
  private static final int MSG_ABORT_TRACE = 3;
  private static final int MSG_END_TRACE = 4;

  // Messages related to upload condition management
  private static final int MSG_EVENT_DURATION = 5;
  private static final int MSG_CONDITIONAL_TRACE_STOP = 6;
  private static final int MSG_EVENT_QPL_ANNOTATION = 7;

  @GuardedBy("this")
  private final @Nullable TraceControl.TraceControlListener mListener;

  private final TraceWriterListener mCallbacks;

  @GuardedBy("this")
  private final HashSet<Long> mTraceContexts;

  @GuardedBy("this")
  private final TraceConditionManager mTraceConditionManager;

  private final Random mRandom;

  public TraceControlHandler(
      @Nullable TraceControl.TraceControlListener listener,
      Looper looper,
      TraceWriterListener callbacks) {
    super(looper);
    mListener = listener;
    mCallbacks = callbacks;
    mTraceContexts = new HashSet<>();
    mTraceConditionManager = new TraceConditionManager();
    mRandom = new Random();
  }

  @Override
  public void handleMessage(Message msg) {
    TraceContext traceContext = null;
    if (msg.what != MSG_EVENT_QPL_ANNOTATION) {
      traceContext = (TraceContext) msg.obj;
    }
    switch (msg.what) {
      case MSG_TIMEOUT_TRACE:
        timeoutTrace(traceContext.traceId);
        break;
      case MSG_START_TRACE:
        startTraceAsync(traceContext);
        break;
      case MSG_STOP_TRACE:
        stopTrace(traceContext);
        break;
      case MSG_END_TRACE:
        endTrace(traceContext);
        break;
      case MSG_ABORT_TRACE:
        abortTrace(traceContext);
        break;
      case MSG_EVENT_DURATION:
        processDurationEvent(traceContext, msg.arg1);
        break;
      case MSG_CONDITIONAL_TRACE_STOP:
        conditionalTraceStop(traceContext);
        break;
      case MSG_EVENT_QPL_ANNOTATION:
        AnnotationContext annotationContext = (AnnotationContext) msg.obj;
        processAnnotationEvent(annotationContext.mTraceContext, annotationContext.mAnnotation);
        break;
      default:
        break;
    }
  }

  protected synchronized void stopTrace(TraceContext context) {
    // stop any timeout timer associated with the thread
    removeMessages(MSG_TIMEOUT_TRACE, context);

    if ((context.flags & Trace.FLAG_MEMORY_ONLY) != 0) {
      // In-memory trace, write the log priority then tell
      // the worker thread to read backwards.

      if (context.workerThread == null) {
        throw new IllegalStateException("Trace stopped but never started");
      }

      context.workerThread.start();
      writeLoggerPriority(context);

      // For in-memory trace everything in the tracing buffer trace is started and then immediately
      // stopped after dumping the contents of the buffer.
      BufferLogger.writeAndWakeupTraceWriter(
          context.workerThread.getTraceWriter(),
          context.mainBuffer,
          context.traceId,
          EntryType.TRACE_BACKWARDS,
          ProfiloConstants.NONE,
          context.flags,
          context.traceId);
    }

    // Schedule an optionally delayed *actual* end of the trace
    sendMessageDelayed(
        obtainMessage(MSG_END_TRACE, context),
        context.mTraceConfigExtras.getIntParam(
            ProfiloConstants.TRACE_CONFIG_PARAM_POST_TRACE_EXTENSION_MSEC,
            ProfiloConstants.TRACE_CONFIG_PARAM_POST_TRACE_EXTENSION_MSEC_DEFAULT));
  }

  protected void conditionalTraceStop(TraceContext context) {
    int uploadSampleRate = 0;
    uploadSampleRate = mTraceConditionManager.getUploadSampleRate(context.traceId);
    if (uploadSampleRate == 0 || mRandom.nextInt(uploadSampleRate) != 0) {
      BufferLogger.writeStandardEntry(
          context.mainBuffer,
          Logger.FILL_TIMESTAMP | Logger.FILL_TID,
          EntryType.TRACE_ABORT,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          context.traceId);
      onTraceAbort(new TraceContext(context, ProfiloConstants.ABORT_REASON_CONDITION_NOT_MET));
    } else {
      BufferLogger.writeStandardEntry(
          context.mainBuffer,
          Logger.FILL_TIMESTAMP | Logger.FILL_TID,
          EntryType.CONDITIONAL_UPLOAD_RATE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          uploadSampleRate);
      BufferLogger.writeStandardEntry(
          context.mainBuffer,
          Logger.FILL_TIMESTAMP | Logger.FILL_TID,
          EntryType.TRACE_PRE_END,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          context.traceId);
      onTraceStop(context);
    }

    mTraceConditionManager.unregisterTrace(context);
  }

  protected void processDurationEvent(TraceContext context, long duration) {
    mTraceConditionManager.processDurationEvent(context.traceId, duration);
  }

  protected void processAnnotationEvent(TraceContext context, String annotation) {
    mTraceConditionManager.processAnnotationEvent(context.traceId, annotation);
  }

  protected void endTrace(TraceContext context) {
    // This also runs teardown for all providers
    if (mListener != null) {
      mListener.onTraceStop(context);
    }
    BufferLogger.writeStandardEntry(
        context.mainBuffer,
        Logger.FILL_TIMESTAMP | Logger.FILL_TID,
        EntryType.TRACE_END,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        context.traceId);
  }

  protected void abortTrace(TraceContext context) {
    // stop any timeout timer associated with the thread
    synchronized (this) {
      removeMessages(MSG_TIMEOUT_TRACE, context);
    }
    if (mListener != null) {
      mListener.onTraceAbort(context);
    }
  }

  protected static void timeoutTrace(long traceId) {
    TraceControl control = TraceControl.get();
    control.timeoutTrace(traceId);
  }

  protected void startTraceAsync(TraceContext context) {

    if ((context.flags & Trace.FLAG_MEMORY_ONLY) == 0) {
      // Normal trace, start the thread now.
      // For in-memory traces, see stopTrace.
      context.workerThread.start();
      writeLoggerPriority(context);
    }

    if (mListener != null) {
      mListener.onTraceStartAsync(context);
    }
  }

  private void writeLoggerPriority(TraceContext context) {
    int logger_priority =
        context.mTraceConfigExtras.getIntParam(
            ProfiloConstants.TRACE_CONFIG_PARAM_LOGGER_PRIORITY,
            ProfiloConstants.TRACE_CONFIG_PARAM_LOGGER_PRIORITY_DEFAULT);

    BufferLogger.writeStandardEntry(
        context.mainBuffer,
        Logger.FILL_TID | Logger.FILL_TIMESTAMP,
        EntryType.LOGGER_PRIORITY,
        0,
        0,
        logger_priority,
        0,
        context.traceId);
  }

  public synchronized void processMarkerStop(TraceContext context, final int eventDuration) {
    Message eventDurationMessage =
        obtainMessage(
            MSG_EVENT_DURATION, /* what */
            eventDuration, /* arg1 */
            0, /* arg2, unused */
            context /* obj */);
    sendMessage(eventDurationMessage);
  }

  public synchronized void processAnnotation(TraceContext context, String annotation) {
    AnnotationContext annotationContext = new AnnotationContext(context, annotation);
    Message annotationMessage =
        obtainMessage(MSG_EVENT_QPL_ANNOTATION, /* what */ annotationContext /* obj */);
    sendMessage(annotationMessage);
  }

  public synchronized void onConditionalTraceStop(TraceContext context) {
    Message stopMessage = obtainMessage(MSG_CONDITIONAL_TRACE_STOP, context);
    sendMessage(stopMessage);
  }

  public synchronized boolean onTraceStart(final TraceContext context, final int timeoutMillis) {
    boolean traceRegistered = mTraceConditionManager.registerTrace(context);

    if (!traceRegistered) {
      // The only way this can happen is if one of the conditions in the config
      // is malformed. Let's abort the trace in this case.
      return false;
    }

    LoggerWorkerThread thread;
    try {
      thread =
          new LoggerWorkerThread(
              context.traceId,
              context.folder.getCanonicalPath(),
              context.prefix,
              context.buffers,
              new NativeTraceWriterCallbacks() {
                @Override
                public void onTraceWriteStart(long traceId, int flags) {
                  mCallbacks.onTraceWriteStart(context);
                }

                @Override
                public void onTraceWriteEnd(long traceId) {
                  mCallbacks.onTraceWriteEnd(context);
                }

                @Override
                public void onTraceWriteAbort(long traceId, int abortReason) {
                  mCallbacks.onTraceWriteAbort(context, abortReason);
                }

                @Override
                public void onTraceWriteException(long traceId, Throwable t) {
                  mCallbacks.onTraceWriteException(context, t);
                }
              });
    } catch (IOException e) {
      throw new IllegalArgumentException(
          "Could not get canonical path of trace directory " + context.folder, e);
    }
    context.workerThread = thread;

    if ((context.flags & Trace.FLAG_MEMORY_ONLY) == 0) {
      // We want to write TRACE_START while we're still synchronously in the startTrace call.
      // This way we can be sure that the worker thread will not miss any entries, as it starts
      // reading from TRACE_START.
      BufferLogger.writeAndWakeupTraceWriter(
          thread.getTraceWriter(),
          context.mainBuffer,
          context.traceId,
          EntryType.TRACE_START,
          TraceControl.getTimeoutFromContext(context),
          context.flags,
          context.traceId);
    }

    mTraceContexts.add(context.traceId);
    if (mListener != null) {
      mListener.onTraceStartSync(context);
    }
    Message startMessage = obtainMessage(MSG_START_TRACE, context);
    sendMessage(startMessage);

    Message timeoutMessage = obtainMessage(MSG_TIMEOUT_TRACE, context);
    sendMessageDelayed(timeoutMessage, timeoutMillis);
    return true;
  }

  public synchronized void onTraceStop(TraceContext context) {
    if (mTraceContexts.contains(context.traceId)) {
      Message stopMessage = obtainMessage(MSG_STOP_TRACE, context);
      sendMessage(stopMessage);
      mTraceContexts.remove(context.traceId);
    }
  }

  public synchronized void onTraceAbort(TraceContext context) {
    if (mTraceContexts.contains(context.traceId)) {
      Message abortMessage = obtainMessage(MSG_ABORT_TRACE, context);
      sendMessage(abortMessage);
      mTraceContexts.remove(context.traceId);
    }
  }
}
