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

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import com.facebook.androidinternals.android.os.SystemPropertiesInternal;
import com.facebook.common.build.BuildConstants;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.Trace;
import java.util.HashSet;
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

  private static final String LOG_TAG = "Profilo/TraceControlThread";
  private static final int MSG_TIMEOUT_TRACE = 0;
  private static final int MSG_START_TRACE = 1;
  private static final int MSG_STOP_TRACE = 2;
  private static final int MSG_ABORT_TRACE = 3;
  // Set this system property to enable logs.
  private static final String PROFILO_LOG_LEVEL_SYSTEM_PROPERTY = "com.facebook.profilo.log";

  @GuardedBy("this") private final @Nullable TraceControl.TraceControlListener mListener;

  @GuardedBy("this")
  private final HashSet<Long> mTraceContexts;

  static class LogLevel {
    // Lazy load system properties.
    private static final boolean LOG_DEBUG_MESSAGE =
        BuildConstants.isInternalBuild()
            || "debug".equals(SystemPropertiesInternal.get(PROFILO_LOG_LEVEL_SYSTEM_PROPERTY));
  }

  public TraceControlHandler(@Nullable TraceControl.TraceControlListener listener, Looper looper) {
    super(looper);
    mListener = listener;
    mTraceContexts = new HashSet<>();
  }

  @Override
  public void handleMessage(Message msg) {
    TraceContext traceContext = (TraceContext) msg.obj;
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
      case MSG_ABORT_TRACE:
        abortTrace(traceContext);
        break;
      default:
        break;
    }
  }

  protected synchronized void stopTrace(TraceContext context) {
    // stop any timeout timer associated with the thread
    removeMessages(MSG_TIMEOUT_TRACE, context);

    if ((context.flags & Trace.FLAG_MEMORY_ONLY) != 0) {
      // For in-memory trace everything in the tracing buffer trace is started and then immediately
      // stopped after dumping the contents of the buffer.
      Logger.postCreateBackwardTrace(context.traceId);
    }

    // call the listener's pre-stop trace and stop trace.
    Logger.postPreCloseTrace(context.traceId);
    if (mListener != null) {
      mListener.onTraceStop(context);
    }
    Logger.postCloseTrace(context.traceId);
  }

  protected synchronized void abortTrace(TraceContext context) {
    // stop any timeout timer associated with the thread
    removeMessages(MSG_TIMEOUT_TRACE, context);
    if (mListener != null) {
      mListener.onTraceAbort(context);
    }
  }

  protected static void timeoutTrace(long traceId) {
    if (LogLevel.LOG_DEBUG_MESSAGE) {
      Log.d(LOG_TAG, "Timing out trace " + traceId);
    }
    TraceControl control = TraceControl.get();
    control.timeoutTrace(traceId);
  }

  protected synchronized void startTraceAsync(TraceContext context) {
    if (LogLevel.LOG_DEBUG_MESSAGE) {
      Log.d(
          LOG_TAG,
          "Started trace " + context.encodedTraceId + "  for controller " + context.controller);
    }
    if (mListener != null) {
      mListener.onTraceStartAsync(context);
    }
  }

  public synchronized void onTraceStart(
      TraceContext context,
      final int timeoutMillis) {

    mTraceContexts.add(context.traceId);
    if (mListener != null) {
      mListener.onTraceStartSync(context);
    }
    Message startMessage = obtainMessage(MSG_START_TRACE, context);
    sendMessage(startMessage);

    Message timeoutMessage = obtainMessage(MSG_TIMEOUT_TRACE, context);
    sendMessageDelayed(timeoutMessage, timeoutMillis);
  }

  public synchronized void onTraceStop(TraceContext context) {
    if (mTraceContexts.contains(context.traceId)) {
      Message stopMessage = obtainMessage(MSG_STOP_TRACE, context);
      sendMessage(stopMessage);
      mTraceContexts.remove(context.traceId);
    }
    if (LogLevel.LOG_DEBUG_MESSAGE) {
      Log.d(LOG_TAG, "Stopped trace " + context.encodedTraceId);
    }
  }

  public synchronized void onTraceAbort(TraceContext context) {
    if (mTraceContexts.contains(context.traceId)) {
      Message abortMessage = obtainMessage(MSG_ABORT_TRACE, context);
      sendMessage(abortMessage);
      mTraceContexts.remove(context.traceId);
    }
    if (LogLevel.LOG_DEBUG_MESSAGE) {
      Log.d(
          LOG_TAG,
          "Aborted trace "
              + context.encodedTraceId
              + " for reason "
              + ProfiloConstants.unpackRemoteAbortReason(context.abortReason)
              + (ProfiloConstants.isRemoteAbort(context.abortReason) ? " (remote process)" : ""));
    }
  }
}
