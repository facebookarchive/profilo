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
package com.facebook.profilo.logger;

import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.core.TraceEvents;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.mmapbuf.Buffer;
import com.facebook.profilo.writer.NativeTraceWriter;

public final class Logger {

  private static volatile boolean sInitialized;

  public static void initialize() {
    sInitialized = true;
  }

  public static final int SKIP_PROVIDER_CHECK = 1 << 0;
  public static final int FILL_TIMESTAMP = 1 << 1;
  public static final int FILL_TID = 1 << 2;

  public static int writeStandardEntry(
      int provider,
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */) {
    if (sInitialized && ((flags & SKIP_PROVIDER_CHECK) != 0 || TraceEvents.isEnabled(provider))) {
      return BufferLogger.writeStandardEntry(null, flags, type, timestamp, tid, arg1, arg2, arg3);
    } else {
      return ProfiloConstants.TRACING_DISABLED;
    }
  }

  public static int writeBytesEntry(
      int provider, int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */) {
    if (sInitialized && ((flags & SKIP_PROVIDER_CHECK) != 0 || TraceEvents.isEnabled(provider))) {
      return BufferLogger.writeBytesEntry(null, flags, type, arg1, arg2);
    } else {
      return ProfiloConstants.TRACING_DISABLED;
    }
  }

  public static void postCreateTrace(
      NativeTraceWriter writer, Buffer buffer, long traceId, int flags, int timeoutMs) {
    BufferLogger.writeAndWakeupTraceWriter(
        writer, buffer, traceId, EntryType.TRACE_START, timeoutMs, flags, traceId);
  }

  public static void postCreateBackwardTrace(
      NativeTraceWriter writer, Buffer buffer, long traceId, int flags) {
    BufferLogger.writeAndWakeupTraceWriter(
        writer, buffer, traceId, EntryType.TRACE_BACKWARDS, 0, flags, traceId);
  }

  public static void postConditionalUploadRate(int uploadRate) {
    writeStandardEntry(
        ProfiloConstants.NONE,
        SKIP_PROVIDER_CHECK | FILL_TIMESTAMP | FILL_TID,
        EntryType.CONDITIONAL_UPLOAD_RATE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        uploadRate);
  }

  public static void postPreCloseTrace(long traceId) {
    postFinishTrace(EntryType.TRACE_PRE_END, traceId);
  }

  public static void postCloseTrace(long traceId) {
    postFinishTrace(EntryType.TRACE_END, traceId);
  }

  public static void postAbortTrace(long traceId) {
    postFinishTrace(EntryType.TRACE_ABORT, traceId);
  }

  public static void postTimeoutTrace(long traceId) {
    postFinishTrace(EntryType.TRACE_TIMEOUT, traceId);
  }

  private static void postFinishTrace(int entryType, long traceId) {
    writeStandardEntry(
        ProfiloConstants.NONE,
        SKIP_PROVIDER_CHECK | FILL_TIMESTAMP | FILL_TID,
        entryType,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        traceId);
  }
}
