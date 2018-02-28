// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.logger;

import android.annotation.SuppressLint;
import com.facebook.loom.core.LoomConstants;
import com.facebook.loom.core.TraceEvents;
import com.facebook.loom.entries.EntryType;
import com.facebook.loom.writer.NativeTraceWriter;
import com.facebook.loom.writer.NativeTraceWriterCallbacks;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.util.concurrent.atomic.AtomicReference;
import javax.annotation.Nullable;

@DoNotStrip
final public class Logger {

  /**
   * A reference to the underlying trace writer.
   * The first time we make this wrapper, we may cause the loom
   * buffer to be allocated, so delay that as much as possible.
   *
   * This variable starts as null but once assigned, it never goes back to null.
   */
  @Nullable private static volatile NativeTraceWriter sTraceWriter;

  private static volatile boolean sInitialized;

  private static AtomicReference<LoggerWorkerThread> mWorker;
  private static File sTraceDirectory;
  private static String sFilePrefix;
  private static NativeTraceWriterCallbacks sNativeTraceWriterCallbacks;
  private static LoggerCallbacks sLoggerCallbacks;
  private static int sRingBufferSize;

  public static void initialize(
      int ringBufferSize,
      File traceDirectory,
      String filePrefix,
      NativeTraceWriterCallbacks nativeTraceWriterCallbacks,
      LoggerCallbacks loggerCallbacks) {
    SoLoader.loadLibrary("profilo");
    TraceEvents.sInitialized = true;

    sInitialized = true;
    sTraceDirectory = traceDirectory;
    sFilePrefix = filePrefix;
    sLoggerCallbacks = loggerCallbacks;
    sNativeTraceWriterCallbacks = nativeTraceWriterCallbacks;
    sRingBufferSize = ringBufferSize;
    mWorker = new AtomicReference<>(null);
  }

  public static int writeEntryWithoutMatch(int provider, int type, int callID) {
    if (!sInitialized) {
      return LoomConstants.TRACING_DISABLED;
    }
    return writeEntryWithCursor(provider, type, callID, LoomConstants.NO_MATCH, 0, null);
  }

  public static int writeEntryWithoutMatch(
      int provider,
      int type,
      int callID,
      long intExtra) {
    if (!sInitialized) {
      return LoomConstants.TRACING_DISABLED;
    }
    return writeEntryWithCursor(
        provider,
        type,
        callID,
        LoomConstants.NO_MATCH,
        intExtra,
        null);
  }

  public static int writeEntryWithoutMatch(
      int provider,
      int type,
      int callID,
      long intExtra,
      long monotonicTime) {
    if (!sInitialized ||
        (provider != LoomConstants.PROVIDER_LOOM_SYSTEM && !TraceEvents.isEnabled(provider))) {
      return LoomConstants.TRACING_DISABLED;
    }
    return loggerWriteWithMonotonicTime(
        type,
        callID,
        LoomConstants.NO_MATCH,
        intExtra,
        monotonicTime);
  }

  /**
   * Logging method for thread specific counters
   */
  public static int writeEntryWithoutMatchForThread(
      int provider,
      int tid,
      int type,
      int callID,
      long intExtra) {
    if (!sInitialized ||
        (provider != LoomConstants.PROVIDER_LOOM_SYSTEM && !TraceEvents.isEnabled(provider))) {
      return LoomConstants.TRACING_DISABLED;
    }

    return loggerWriteForThread(tid, type, callID, LoomConstants.NO_MATCH, intExtra);
  }

  public static int writeEntry(int provider, int type, int callID, int matchID) {
    if (!sInitialized) {
      return LoomConstants.TRACING_DISABLED;
    }
    return writeEntryWithCursor(
        provider,
        type,
        callID,
        matchID,
        0,
        null);
  }

  public static int writeEntry(
      int provider,
      int type,
      int callID,
      int matchID,
      long intExtra) {
    if (!sInitialized) {
      return LoomConstants.TRACING_DISABLED;
    }
    return writeEntryWithCursor(
        provider,
        type,
        callID,
        matchID,
        intExtra,
        null);
  }

  public static int writeEntry(
      int provider,
      int type,
      int matchID,
      String str) {
    if (!sInitialized) {
      return LoomConstants.TRACING_DISABLED;
    }
    return writeEntryWithCursor(
        provider,
        type,
        0,
        matchID,
        0,
        str);
  }

  public static int writeEntryWithString(
      int provider,
      int type,
      int callID,
      int matchID,
      long intExtra,
      @Nullable String strKey,
      String strValue) {

    // Early bailout for disabled providers too to avoid generating the
    // STRING_VALUE entries.
    if (!sInitialized ||
        (provider != LoomConstants.PROVIDER_LOOM_SYSTEM && !TraceEvents.isEnabled(provider))) {
      return LoomConstants.TRACING_DISABLED;
    }
    int returnedMatchID = writeEntryWithCursor(
        provider,
        type,
        callID,
        matchID,
        intExtra,
        null);
    return writeKeyValueStringWithMatch(provider, returnedMatchID, strKey, strValue);
  }

  public static int writeEntryWithStringWithNoMatch(
      int provider,
      int type,
      int callID,
      long intExtra,
      @Nullable String strKey,
      String strValue) {
    if (!sInitialized ||
        (provider != LoomConstants.PROVIDER_LOOM_SYSTEM && !TraceEvents.isEnabled(provider))) {
      return LoomConstants.TRACING_DISABLED;
    }
    int returnedMatchID = writeEntryWithCursor(
        provider,
        type,
        callID,
        LoomConstants.NO_MATCH,
        intExtra,
        null);
    return writeKeyValueStringWithMatch(provider, returnedMatchID, strKey, strValue);
  }

  public static int writeEntryWithStringWithNoMatch(
      int provider,
      int type,
      int callID,
      long intExtra,
      @Nullable String strKey,
      String strValue,
      long monotonicTime) {
    if (!sInitialized ||
        (provider != LoomConstants.PROVIDER_LOOM_SYSTEM && !TraceEvents.isEnabled(provider))) {
      return LoomConstants.TRACING_DISABLED;
    }
    int returnedMatchID = loggerWriteWithMonotonicTime(
        type,
        callID,
        LoomConstants.NO_MATCH,
        intExtra,
        monotonicTime);
    return writeKeyValueStringWithMatch(provider, returnedMatchID, strKey, strValue);
  }

  public static int writeKeyValueStringWithMatch(
      int provider,
      int matchID,
      @Nullable String strKey,
      String strValue) {

    if (strKey != null) {
      matchID = writeEntry(
          provider,
          EntryType.STRING_KEY,
          matchID,
          strKey);
    }
    return writeEntry(
        provider,
        EntryType.STRING_VALUE,
        matchID,
        strValue);
  }

  public static int writeStringKeyValueClassWithMatch(
      int provider,
      int matchID,
      String strKey,
      long clsId) {
    matchID = writeEntry(
        provider,
        EntryType.STRING_KEY,
        matchID,
        strKey);

    return writeEntry(
        provider,
        EntryType.CLASS_VALUE,
        0,
        matchID,
        clsId);
  }

  public static void postCreateTrace(long traceId, int flags, int timeoutMs) {
    if (sInitialized) {
      startWorkerThreadIfNecessary();

      loggerWriteAndWakeupTraceWriter(
          sTraceWriter,
          traceId,
          EntryType.TRACE_START,
          timeoutMs,
          flags,
          traceId);
    }
  }

  public static void postCreateBackwardTrace(long traceId) {
    if (sInitialized) {
      startWorkerThreadIfNecessary();

      loggerWriteAndWakeupTraceWriter(
          sTraceWriter, traceId, EntryType.TRACE_BACKWARDS, 0, LoomConstants.NO_MATCH, traceId);
    }
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
    if (sInitialized) {
      writeEntryWithCursor(
          LoomConstants.PROVIDER_LOOM_SYSTEM,
          entryType,
          LoomConstants.NO_CALLSITE,
          LoomConstants.NO_MATCH,
          traceId,
          null);
    }
  }

  private static native int loggerWrite(
      int type,
      int callid,
      int matchid,
      long extra);

  private static native int loggerWriteWithMonotonicTime(
      int type,
      int callid,
      int matchid,
      long extra,
      long monotonicTime);

  private static native int loggerWriteForThread(
      int tid,
      int type,
      int callid,
      int matchid,
      long extra);

  private static native int loggerWriteAndWakeupTraceWriter(
      NativeTraceWriter writer,
      long traceId,
      int type,
      int callid,
      int matchid,
      long extra);

  private static native int loggerWriteString(int type, int matchid, String str);

  private static int writeEntryWithCursor(
      int provider,
      int type,
      int callID,
      int matchID,
      long extra,
      @Nullable String str) {

    if (provider != LoomConstants.PROVIDER_LOOM_SYSTEM && !TraceEvents.isEnabled(provider))  {
      return LoomConstants.TRACING_DISABLED;
    }

    if (str != null) {
      return loggerWriteString(type, matchID, str);
    }
    return loggerWrite(type, callID, matchID, extra);
  }

  @SuppressLint("BadMethodUse-java.lang.Thread.start")
  private static void startWorkerThreadIfNecessary() {
    if (mWorker.get() != null) {
      // race conditions make it possible to try to start a second thread. ignore the call.
      return;
    }

    NativeTraceWriter writer =
        new NativeTraceWriter(
            sTraceDirectory.getAbsolutePath(),
            sFilePrefix,
            sRingBufferSize,
            sNativeTraceWriterCallbacks);

    LoggerWorkerThread thread = new LoggerWorkerThread(writer);

    if (!mWorker.compareAndSet(null, thread)) {
      return;
    }

    sTraceWriter = writer;
    thread.setUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
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
