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

import android.os.StrictMode;
import android.util.Log;
import android.util.SparseArray;
import com.facebook.fbtrace.utils.FbTraceId;
import com.facebook.profilo.config.Config;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceConfigExtras;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.BufferLogger;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.profilo.mmapbuf.core.MmapBufferManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceArray;
import javax.annotation.Nullable;
import javax.annotation.concurrent.NotThreadSafe;

/**
 * Responsible for managing the current trace state - starting, stopping traces based on a config.
 */
@SuppressWarnings({"BadMethodUse-android.util.Log.w"})
public final class TraceControl {

  public static final String LOG_TAG = "Profilo/TraceControl";
  public static final int MAX_TRACES = 2;
  public static final int TRACE_TIMEOUT_MS = 30000; // 30s

  private static final int NORMAL_TRACE_MASK = 0xfffe;
  private static final int MEMORY_TRACE_MASK = 0x1;
  private static final int DEFAULT_RING_BUFFER_SIZE = 5000;

  @NotThreadSafe
  public interface TraceControlListener {

    void onTraceStartSync(TraceContext context);

    void onTraceStartAsync(TraceContext context);

    void onTraceStop(TraceContext context);

    void onTraceAbort(TraceContext context);
  }

  private @interface TraceStopReason {
    int ABORT = 0;
    int STOP = 1;
  }

  private static volatile TraceControl sInstance = null;

  private static final ThreadLocal<Random> sTraceIdRandom =
      new ThreadLocal<Random>() {

        @Override
        public Random initialValue() {
          // Produce an RNG that's not seeded with the current time.

          // StrictMode is disabled because this is a non-blocking read and
          // thus it's not as evil as it thinks.
          StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
          try (FileInputStream input = new FileInputStream("/dev/urandom")) {
            ByteBuffer buffer = ByteBuffer.allocate(8);
            input.read(buffer.array());
            return new Random(buffer.getLong());
          } catch (IOException e) {
            throw new RuntimeException("Cannot read from /dev/urandom", e);
          } finally {
            StrictMode.setThreadPolicy(oldPolicy);
          }
        }
      };

  /*package*/ static void initialize(
      SparseArray<TraceController> controllers,
      @Nullable TraceControlListener listener,
      @Nullable Config initialConfig,
      MmapBufferManager bufferManager,
      File folder,
      String processName,
      TraceWriterListener callbacks) {

    // Use double-checked locking to avoid using AtomicReference and thus increasing the
    // overhead of each read by adding a virtual call to it.
    if (sInstance == null) {
      synchronized (TraceControl.class) {
        if (sInstance == null) {
          sInstance =
              new TraceControl(
                  controllers,
                  initialConfig,
                  listener,
                  bufferManager,
                  folder,
                  processName,
                  callbacks);
        } else {
          throw new IllegalStateException("TraceControl already initialized");
        }
      }
    } else {
      throw new IllegalStateException("TraceControl already initialized");
    }
  }

  @Nullable
  public static TraceControl get() {
    return sInstance;
  }

  private final SparseArray<TraceController> mControllers;

  private final AtomicReferenceArray<TraceContext> mCurrentTraces;
  private final AtomicReference<Config> mCurrentConfig;
  private final MmapBufferManager mBufferManager;
  private final File mFolder;
  private final String mProcessName;
  private final TraceWriterListener mCallbacks;
  private final AtomicInteger mCurrentTracesMask;
  @Nullable private final TraceControlListener mListener;
  @Nullable private TraceControlHandler mTraceControlHandler;

  // VisibleForTesting
  /*package*/ TraceControl(
      SparseArray<TraceController> controllers,
      @Nullable Config config,
      @Nullable TraceControlListener listener,
      MmapBufferManager bufferManager,
      File folder,
      String processName,
      TraceWriterListener callbacks) {
    mControllers = controllers;
    mCurrentConfig = new AtomicReference<>(config);
    mBufferManager = bufferManager;
    mFolder = folder;
    mProcessName = processName;
    mCallbacks = callbacks;
    mCurrentTraces = new AtomicReferenceArray<>(MAX_TRACES);
    mCurrentTracesMask = new AtomicInteger(0);
    mListener = listener;
  }

  public void setConfig(@Nullable Config config) {
    Config oldConfig = mCurrentConfig.get();
    if (!mCurrentConfig.compareAndSet(oldConfig, config)) {
      Log.d(LOG_TAG, "Tried to update the config and failed due to CAS");
    }
  }

  /**
   * Returns lowest available bit from the supplied bit mask It takes trace flags mask into account
   * to adjust for "Memory-only" traces with {@link Trace#FLAG_MEMORY_ONLY} flag. Memory-only traces
   * have the first bit reserved only for them. Other normal traces share the rest of bits for
   * simultaneous tracing.
   *
   * @param bitmask bit mask to check
   * @param maxBits Maximum number of bit slots available
   * @param flags Trace flags mask
   * @return value with lowest free bit set to 1 and 0 if no empty slots left according to maxBits
   */
  private static int findLowestFreeBit(int bitmask, int maxBits, int flags) {
    bitmask |= (flags & Trace.FLAG_MEMORY_ONLY) != 0 ? NORMAL_TRACE_MASK : MEMORY_TRACE_MASK;
    return ((bitmask + 1) & ~bitmask) & ((1 << maxBits) - 1);
  }

  private static int findHighestBitIndex(int bitmask) {
    int i = -1;
    while (bitmask != 0) {
      i++;
      bitmask >>= 1;
    }
    return i;
  }

  @Nullable
  private TraceContext findCurrentTraceByTraceId(long traceId) {
    if (mCurrentTracesMask.get() == 0) {
      return null;
    }
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      if (ctx.traceId == traceId) {
        return ctx;
      }
    }
    return null;
  }

  @Nullable
  public TraceContext getTraceContextByTrigger(
      int controllerMask, long longContext, @Nullable Object context) {
    TraceContext tcx = findCurrentTraceByContext(controllerMask, longContext, context);
    if (tcx == null) {
      return null;
    }
    return new TraceContext(tcx);
  }

  // Notion of a trigger with a unique identifier would help a lot.
  @Nullable
  private TraceContext findCurrentTraceByContext(
      int controllerMask, long longContext, @Nullable Object context) {
    if (mCurrentTracesMask.get() == 0) {
      return null;
    }
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      if ((ctx.controller & controllerMask) == 0) {
        // The context doesn't match the controller mask, continue searching
        continue;
      }

      Object controllerObject = ctx.controllerObject;
      if (((TraceController) controllerObject)
          .contextsEqual(ctx.longContext, ctx.context, longContext, context)) {
        return ctx;
      }
    }
    return null;
  }

  private void removeTraceContext(TraceContext ctx) {
    boolean reset = false;
    for (int i = 0; i < MAX_TRACES; i++) {
      if (mCurrentTraces.compareAndSet(i, ctx, null)) {
        int currentMask;
        do {
          currentMask = mCurrentTracesMask.get();
        } while (!mCurrentTracesMask.compareAndSet(currentMask, currentMask ^ (1 << i)));
        reset = true;
        break;
      }
    }
    if (!reset) {
      Log.w(LOG_TAG, "Could not reset Trace Context to null");
    }
  }

  /**
   * Returns the current TraceContexts
   *
   * @return the current TraceContext. Returns empty list if we are not in a trace.
   */
  public List<TraceContext> getCurrentTraces() {
    if (mCurrentTracesMask.get() == 0) {
      return Collections.emptyList();
    }
    ArrayList<TraceContext> traces = new ArrayList<>(MAX_TRACES);
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      traces.add(new TraceContext(ctx));
    }
    return traces;
  }

  public File getTraceFolder(String trace_id) {
    return new File(mFolder, TraceIdSanitizer.sanitizeTraceId(trace_id));
  }

  public boolean startTrace(int controller, int flags, @Nullable Object context, long longContext) {
    if (findLowestFreeBit(mCurrentTracesMask.get(), MAX_TRACES, flags) == 0) {
      // Reached max traces
      return false;
    }

    TraceController traceController = mControllers.get(controller);
    if (traceController == null) {
      throw new IllegalArgumentException("Unregistered controller for id = " + controller);
    }

    TraceContext traceContext = findCurrentTraceByContext(controller, longContext, context);
    if (traceContext != null) {
      // Attempted start during a trace with the same Id
      return false;
    }

    Config config = mCurrentConfig.get();
    int traceConfigIdx = -1;
    int providers = 0;

    if (!traceController.isConfigurable()) {
      // Special path for non-configurable controllers
      providers = traceController.getNonConfigurableProviders(longContext, context);
    } else if (config != null) {
      int result = traceController.findTraceConfigIdx(longContext, context, config);
      if (result >= 0) {
        traceConfigIdx = result;
        String[] providerStrings = config.getTraceConfigProviders(traceConfigIdx);
        providers = ProvidersRegistry.getBitMaskFor(Arrays.asList(providerStrings));
      } else {
        return false;
      }
    }

    if (providers == 0) {
      return false;
    }

    long traceId = nextTraceID();
    String encodedTraceId = FbTraceId.encode(traceId);
    Log.w(LOG_TAG, "START PROFILO_TRACEID: " + encodedTraceId);

    final TraceConfigExtras traceConfigExtras;
    if (traceController.isConfigurable()) {
      traceConfigExtras = new TraceConfigExtras(config, traceConfigIdx);
    } else {
      // non-configurable controller path
      traceConfigExtras = traceController.getNonConfigurableTraceConfigExtras(longContext, context);
    }

    Buffer[] buffers = getBuffers(config, traceConfigExtras);
    TraceContext nextContext =
        new TraceContext(
            traceId,
            encodedTraceId,
            config,
            controller,
            traceController,
            context,
            longContext,
            providers,
            flags,
            traceConfigIdx,
            traceConfigExtras,
            buffers[0],
            buffers,
            getTraceFolder(encodedTraceId),
            mProcessName);

    return startTraceInternal(flags, nextContext);
  }

  public boolean adoptContext(int controller, int flags, TraceContext traceContext) {
    TraceController traceController = mControllers.get(controller);
    if (traceController == null) {
      throw new IllegalArgumentException("Unregistered controller for id = " + controller);
    }

    Config config = mCurrentConfig.get();
    Buffer[] buffers = getBuffers(config, traceContext.mTraceConfigExtras);
    return startTraceInternal(
        flags,
        new TraceContext(
            traceContext.traceId,
            traceContext.encodedTraceId,
            config,
            controller,
            traceController,
            traceContext.context,
            traceContext.longContext,
            traceContext.enabledProviders,
            traceContext.flags,
            traceContext.abortReason,
            traceContext.traceConfigIdx,
            traceContext.mTraceConfigExtras,
            buffers[0],
            buffers,
            getTraceFolder(traceContext.encodedTraceId),
            mProcessName));
  }

  private Buffer[] getBuffers(Config config, TraceConfigExtras traceConfigExtras) {
    int bufferCount = traceConfigExtras.getIntParam("trace_config.buffers", 1);
    int systemBufferSize =
        config.optSystemConfigParamInt("system_config.buffer_size", DEFAULT_RING_BUFFER_SIZE);
    boolean filebacked = traceConfigExtras.getBoolParam("trace_config.mmap_buffer", false);
    int[] bufferSizes = traceConfigExtras.getIntArrayParam("trace_config.buffer_sizes");

    Buffer[] buffers = new Buffer[bufferCount];
    for (int idx = 0; idx < bufferCount; idx++) {
      buffers[idx] =
          mBufferManager.allocateBuffer(
              bufferSizes != null && idx < bufferSizes.length ? bufferSizes[idx] : systemBufferSize,
              filebacked);
    }
    return buffers;
  }

  private boolean startTraceInternal(int flags, TraceContext nextContext) {
    if (nextContext.buffers == null
        || nextContext.buffers.length == 0
        || nextContext.mainBuffer == null) {
      // Something has gone wrong, fail this startTrace request.
      Log.e(
          LOG_TAG,
          "No buffer was allocated for trace "
              + nextContext.encodedTraceId
              + ", failing startTrace");
      return false;
    }

    while (true) { // Store new trace in CAS manner
      int currentMask = mCurrentTracesMask.get();
      int freeBit = findLowestFreeBit(currentMask, MAX_TRACES, flags);
      if (freeBit == 0) {
        // Reached max traces
        Log.d(LOG_TAG, "Tried to start a trace and failed because no free slots were left");
        return false;
      }
      if (mCurrentTracesMask.compareAndSet(currentMask, currentMask | freeBit)) {
        if (!mCurrentTraces.compareAndSet(findHighestBitIndex(freeBit), null, nextContext)) {
          throw new RuntimeException("ORDERING VIOLATION - ACQUIRED SLOT BUT SLOT NOT EMPTY");
        }
        break;
      }
    }

    for (Buffer buffer : nextContext.buffers) {
      buffer.updateHeader(
          nextContext.enabledProviders,
          nextContext.longContext,
          nextContext.traceId,
          nextContext.config.getID());
    }

    int timeout = getTimeoutFromContext(nextContext);

    boolean trace_started = true;

    synchronized (this) {
      ensureHandlerInitialized();
      // It's a guard if trace stop was initiated from another thread.
      if (findCurrentTraceByTraceId(nextContext.traceId) != null) {
        trace_started = mTraceControlHandler.onTraceStart(nextContext, timeout);
      }
    }

    if (!trace_started) {
      Log.e(
          LOG_TAG,
          "Failed to start trace "
              + nextContext.encodedTraceId
              + " due to malformed config for context "
              + nextContext.longContext);
    }

    return trace_started;
  }

  static int getTimeoutFromContext(TraceContext context) {
    int flags = context.flags;
    int timeout;
    if ((flags & (Trace.FLAG_MANUAL | Trace.FLAG_MEMORY_ONLY)) != 0) {
      // manual and in-memory traces should not time out
      timeout = Integer.MAX_VALUE;
    } else {
      timeout =
          context.mTraceConfigExtras.getIntParam(
              ProfiloConstants.TRACE_CONFIG_PARAM_TRACE_TIMEOUT_MS, TRACE_TIMEOUT_MS);
    }
    return timeout;
  }

  public void processMarkerStop(
      int controllerMask, @Nullable Object context, long longContext, int eventDuration) {
    TraceContext traceContext = findCurrentTraceByContext(controllerMask, longContext, context);
    if (traceContext == null) {
      return;
    }
    synchronized (this) {
      ensureHandlerInitialized();
      mTraceControlHandler.processMarkerStop(traceContext, eventDuration);
    }
  }

  public void processAnnotation(
      int controllerMask, @Nullable Object context, long longContext, String annotation) {
    TraceContext traceContext = findCurrentTraceByContext(controllerMask, longContext, context);
    if (traceContext == null) {
      return;
    }
    synchronized (this) {
      ensureHandlerInitialized();
      mTraceControlHandler.processAnnotation(traceContext, annotation);
    }
  }

  public boolean conditionalTraceStop(
      int controllerMask, @Nullable Object context, long longContext) {
    TraceContext traceContext = findCurrentTraceByContext(controllerMask, longContext, context);
    if (traceContext == null) {
      return false;
    }
    removeTraceContext(traceContext);
    Log.w(LOG_TAG, "STOP PROFILO_TRACEID: " + FbTraceId.encode(traceContext.traceId));
    synchronized (this) {
      ensureHandlerInitialized();
      mTraceControlHandler.onConditionalTraceStop(traceContext);
    }

    return true;
  }

  public boolean stopTrace(int controllerMask, @Nullable Object context, long longContext) {
    return stopTrace(controllerMask, context, TraceStopReason.STOP, longContext, /*abortReason*/ 0);
  }

  public void abortTrace(int controllerMask, @Nullable Object context, long longContext) {
    stopTrace(
        controllerMask,
        context,
        TraceStopReason.ABORT,
        longContext,
        ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);
  }

  public void abortTraceWithReason(
      String trigger, @Nullable Object context, long longContext, int abortReason) {
    abortTraceWithReason(TriggerRegistry.getBitMaskFor(trigger), context, longContext, abortReason);
  }

  public void abortTraceWithReason(
      int controllerMask, @Nullable Object context, long longContext, int abortReason) {
    stopTrace(controllerMask, context, TraceStopReason.ABORT, longContext, abortReason);
  }

  /**
   * Adds timeout entry to the current trace. It is supposed to be called only from {@link
   * LoggerWorkerThread}, not from "frontend" classes.
   */
  void timeoutTrace(long traceId) {
    TraceContext traceContext = findCurrentTraceByTraceId(traceId);
    if (traceContext == null) {
      return;
    }
    BufferLogger.writeStandardEntry(
        traceContext.mainBuffer,
        Logger.FILL_TIMESTAMP | Logger.FILL_TID,
        EntryType.TRACE_TIMEOUT,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        traceContext.traceId);

    cleanupTraceContextByID(traceId, ProfiloConstants.ABORT_REASON_TIMEOUT);
  }

  private boolean stopTrace(
      int controllerMask,
      @Nullable Object context,
      @TraceStopReason int stopReason,
      long longContext,
      int abortReason) {
    TraceContext traceContext = findCurrentTraceByContext(controllerMask, longContext, context);
    if (traceContext == null) {
      // no trace, nothing to do
      return false;
    }

    removeTraceContext(traceContext);

    Log.w(LOG_TAG, "STOP PROFILO_TRACEID: " + FbTraceId.encode(traceContext.traceId));
    synchronized (this) {
      ensureHandlerInitialized();
      switch (stopReason) {
        case TraceStopReason.ABORT:
          BufferLogger.writeStandardEntry(
              traceContext.mainBuffer,
              Logger.FILL_TIMESTAMP | Logger.FILL_TID,
              EntryType.TRACE_ABORT,
              ProfiloConstants.NONE,
              ProfiloConstants.NONE,
              ProfiloConstants.NONE,
              ProfiloConstants.NONE,
              traceContext.traceId);

          mTraceControlHandler.onTraceAbort(new TraceContext(traceContext, abortReason));
          break;
        case TraceStopReason.STOP:
          BufferLogger.writeStandardEntry(
              traceContext.mainBuffer,
              Logger.FILL_TIMESTAMP | Logger.FILL_TID,
              EntryType.TRACE_PRE_END,
              ProfiloConstants.NONE,
              ProfiloConstants.NONE,
              ProfiloConstants.NONE,
              ProfiloConstants.NONE,
              traceContext.traceId);

          mTraceControlHandler.onTraceStop(traceContext);
          break;
      }
    }
    return true;
  }

  /**
   * Internal method to ensure consistency for traces we know are aborted in the logger thread
   * through a path that didn't go through us (e.g., timeout).
   *
   * <p>This does not write an abort trace event into the trace.
   *
   * @param id numeric trace id
   */
  /*package*/ void cleanupTraceContextByID(long id, int abortReason) {
    TraceContext traceContext = findCurrentTraceByTraceId(id);
    if (traceContext == null || traceContext.traceId != id) {
      return;
    }
    removeTraceContext(traceContext);
    synchronized (this) {
      ensureHandlerInitialized();
      mTraceControlHandler.onTraceAbort(new TraceContext(traceContext, abortReason));
    }
  }

  public boolean isInsideTrace() {
    return mCurrentTracesMask.get() != 0;
  }

  public boolean isInsideNormalTrace() {
    return (mCurrentTracesMask.get() & NORMAL_TRACE_MASK) != 0;
  }

  public boolean isInsideMemoryOnlyTrace() {
    return (mCurrentTracesMask.get() & MEMORY_TRACE_MASK) != 0;
  }

  public boolean isInsideTriggerQPLTrace(int markerId) {
    return getCurrentTraceEncodedIdByTriggerQPL(markerId) != null;
  }

  @Nullable
  public String getCurrentTraceEncodedIdByTriggerQPL(int markerId) {
    if (mCurrentTracesMask.get() == 0) {
      return null;
    }
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      if (ctx.controllerObject instanceof ControllerWithQPLChecks
          && ((ControllerWithQPLChecks) ctx.controllerObject)
              .isInsideQPLTrace(ctx.longContext, ctx.context, markerId)) {
        return ctx.encodedTraceId;
      }
    }
    return null;
  }

  @Nullable
  public String getCurrentTraceEncodedIdByTrigger(
      int controller, long longContext, @Nullable Object context) {
    TraceContext ctx = findCurrentTraceByContext(controller, longContext, context);
    if (ctx == null) {
      return null;
    }
    return ctx.encodedTraceId;
  }

  public long getCurrentTraceIdByTrigger(
      int controller, long longContext, @Nullable Object context) {
    TraceContext ctx = findCurrentTraceByContext(controller, longContext, context);
    if (ctx == null) {
      return 0;
    }
    return ctx.traceId;
  }

  /** Return some context about the triggering controller and context. */
  @Nullable
  public String[] getTriggerContextStrings() {
    if (mCurrentTracesMask.get() == 0) {
      return null;
    }
    String[] contextStrings = new String[MAX_TRACES];
    int index = 0;
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      contextStrings[index++] =
          "Context: "
              + ctx.toString()
              + " ControllerObject: "
              + ctx.controllerObject.toString()
              + " LongContext: "
              + Long.toString(ctx.longContext);
    }
    if (index == 0) {
      return null;
    }
    return Arrays.copyOf(contextStrings, index);
  }

  /**
   * ONLY USE WITHIN com.facebook.profilo.* (Visibility cannot be restricted, must be usable from
   * outside profilo.core)
   */
  public int getCurrentTraceControllers() {
    if (mCurrentTracesMask.get() == 0) {
      return 0;
    }
    int controllers = 0;
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      controllers |= ctx.controller;
    }
    return controllers;
  }

  /** @return current trace ID or 0 if not inside a trace */
  @Nullable
  public long[] getCurrentTraceIDs() {
    if (mCurrentTracesMask.get() == 0) {
      return null;
    }
    long[] traceIds = new long[MAX_TRACES];
    int index = 0;
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      traceIds[index++] = ctx.traceId;
    }
    if (index == 0) {
      return null;
    }
    return Arrays.copyOf(traceIds, index);
  }

  public Config getConfig() {
    return mCurrentConfig.get();
  }

  @Nullable
  public TraceController getController(int controller) {
    return mControllers.get(controller);
  }

  /** @return current fbtrace encoded trace ID or 'AAAAAAAAAAA' (encoded 0) if not inside a trace */
  @Nullable
  public String[] getEncodedCurrentTraceIDs() {
    if (mCurrentTracesMask.get() == 0) {
      return null;
    }
    String[] traceIds = new String[MAX_TRACES];
    int index = 0;
    for (int i = 0; i < MAX_TRACES; i++) {
      TraceContext ctx = mCurrentTraces.get(i);
      if (ctx == null) {
        continue;
      }
      traceIds[index++] = ctx.encodedTraceId;
    }
    if (index == 0) {
      return null;
    }
    return Arrays.copyOf(traceIds, index);
  }

  private static long nextTraceID() {
    long l;
    // Math.abs(Long.MIN_VALUE) is still negative. Thanks, java.
    while ((l = Math.abs(sTraceIdRandom.get().nextLong())) <= 0) ;
    return l;
  }

  private void ensureHandlerInitialized() {
    if (mTraceControlHandler == null) {
      mTraceControlHandler =
          new TraceControlHandler(
              mListener, TraceControlThreadHolder.getInstance().getLooper(), mCallbacks);
    }
  }
}
