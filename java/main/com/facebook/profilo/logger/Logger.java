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

import android.annotation.SuppressLint;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.core.TraceEvents;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.mmapbuf.Buffer;
import com.facebook.profilo.mmapbuf.MmapBufferManager;
import com.facebook.profilo.writer.NativeTraceWriter;
import com.facebook.profilo.writer.NativeTraceWriterCallbacks;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;
import javax.annotation.Nullable;

@DoNotStrip
public final class Logger {

  /**
   * A reference to the underlying trace writer. The first time we make this wrapper, we may cause
   * the profilo buffer to be allocated, so delay that as much as possible.
   *
   * <p>This variable starts as null but once assigned, it never goes back to null.
   */
  @Nullable private static volatile NativeTraceWriter sTraceWriter;

  private static volatile boolean sInitialized;

  private static AtomicReference<LoggerWorkerThread> sWorker;
  private static File sTraceDirectory;
  private static String sFilePrefix;
  private static NativeTraceWriterCallbacks sNativeTraceWriterCallbacks;
  private static LoggerCallbacks sLoggerCallbacks;
  private static int sRingBufferSize;
  private static MmapBufferManager sMmapBufferManager;
  private static boolean sMmappedBuffer;

  public static void initialize(
      int ringBufferSize,
      File traceDirectory,
      String filePrefix,
      NativeTraceWriterCallbacks nativeTraceWriterCallbacks,
      LoggerCallbacks loggerCallbacks,
      MmapBufferManager mmapBufferManager,
      boolean fileBackedBuffer) {
    SoLoader.loadLibrary("profilo");
    TraceEvents.sInitialized = true;

    sInitialized = true;
    sTraceDirectory = traceDirectory;
    sFilePrefix = filePrefix;
    sLoggerCallbacks = loggerCallbacks;
    sNativeTraceWriterCallbacks = nativeTraceWriterCallbacks;
    sRingBufferSize = ringBufferSize;
    sWorker = new AtomicReference<>(null);
    sMmapBufferManager = mmapBufferManager;
    sMmappedBuffer = fileBackedBuffer;
  }

  public static void stopTraceWriter() {
    if (sTraceWriter != null) {
      stopTraceWriter(sTraceWriter);
    }
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
      return loggerWriteStandardEntry(flags, type, timestamp, tid, arg1, arg2, arg3);
    } else {
      return ProfiloConstants.TRACING_DISABLED;
    }
  }

  public static int writeBytesEntry(
      int provider, int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */) {
    if (arg2 == null) {
      // Null strings will break the native side. One option here is to just
      // avoid them and return TRACING_DISABLED. However, doing so would mess
      // with the event stream, that is, it would affect subsequent entries
      // that depend on "this" write. Thus, write something meaningless, just
      // so that we can keep the correct sequence of events
      arg2 = "null";
    }
    if (sInitialized && ((flags & SKIP_PROVIDER_CHECK) != 0 || TraceEvents.isEnabled(provider))) {
      return loggerWriteBytesEntry(flags, type, arg1, arg2);
    } else {
      return ProfiloConstants.TRACING_DISABLED;
    }
  }

  public static void postCreateTrace(long traceId, int flags, int timeoutMs) {
    if (!sInitialized) {
      return;
    }

    Buffer buffer = null;
    if (sMmappedBuffer) {
      // If mmap buffer mode is enabled but allocation fails Ring Buffer will be initialized
      // using default initializer.
      buffer = sMmapBufferManager.allocateBuffer(sRingBufferSize, true);
    }
    if (buffer == null) {
      buffer = sMmapBufferManager.allocateBuffer(sRingBufferSize, false);
    }

    // Do not trigger trace writer for memory-only trace
    if ((flags & Trace.FLAG_MEMORY_ONLY) != 0) {
      return;
    }

    startWorkerThreadIfNecessary(buffer);
    loggerWriteAndWakeupTraceWriter(
        sTraceWriter, traceId, EntryType.TRACE_START, timeoutMs, flags, traceId);
  }

  public static void postCreateBackwardTrace(long traceId, int flags) {
    if (sInitialized) {
      startWorkerThreadIfNecessary(sMmapBufferManager.getBuffer());

      loggerWriteAndWakeupTraceWriter(
          sTraceWriter, traceId, EntryType.TRACE_BACKWARDS, 0, flags, traceId);
    }
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

  private static native int loggerWriteStandardEntry(
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */);

  private static native int loggerWriteBytesEntry(
      int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */);

  private static native int loggerWriteAndWakeupTraceWriter(
      NativeTraceWriter writer, long traceId, int type, int callid, int matchid, long extra);

  public static native void stopTraceWriter(NativeTraceWriter writer);

  public static void updateMmapBufferSessionId(String sessionId) {
    MmapBufferManager mmapBufferManager = sMmapBufferManager;
    if (mmapBufferManager == null) {
      return;
    }
    mmapBufferManager.updateId(sessionId);
  }

  @SuppressLint("BadMethodUse-java.lang.Thread.start")
  private static void startWorkerThreadIfNecessary(Buffer buffer) {
    if (sWorker.get() != null) {
      // race conditions make it possible to try to start a second thread. ignore the call.
      return;
    }

    NativeTraceWriter writer;
    try {
      writer =
          new NativeTraceWriter(
              buffer, sTraceDirectory.getCanonicalPath(), sFilePrefix, sNativeTraceWriterCallbacks);
    } catch (IOException e) {
      throw new IllegalArgumentException("Could not get canonical path of trace directory");
    }

    LoggerWorkerThread thread = new LoggerWorkerThread(writer);

    if (!sWorker.compareAndSet(null, thread)) {
      return;
    }

    sTraceWriter = writer;
    thread.setUncaughtExceptionHandler(
        new Thread.UncaughtExceptionHandler() {
          @Override
          public void uncaughtException(Thread thread, Throwable ex) {
            LoggerCallbacks callbacks = sLoggerCallbacks;
            if (callbacks != null) {
              callbacks.onLoggerException(ex);
            }
          }
        });

    thread.start();
  }
}
