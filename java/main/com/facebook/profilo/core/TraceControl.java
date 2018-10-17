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

import android.os.StrictMode;
import android.util.Log;
import android.util.SparseArray;
import com.facebook.fbtrace.utils.FbTraceId;
import com.facebook.profilo.config.Config;
import com.facebook.profilo.config.ControllerConfig;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.Trace;
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
@SuppressWarnings({
    "BadMethodUse-android.util.Log.w"})
final public class TraceControl {

  public static final String LOG_TAG = "Profilo/TraceControl";
  public static final int MAX_TRACES = 2;
  private static final int TRACE_TIMEOUT_MS = 30000; // 30s

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

  private static final ThreadLocal<Random> sTraceIdRandom = new ThreadLocal<Random>() {

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
      @Nullable Config initialConfig) {

    // Use double-checked locking to avoid using AtomicReference and thus increasing the
    // overhead of each read by adding a virtual call to it.
    if (sInstance == null) {
      synchronized (TraceControl.class) {
        if (sInstance == null) {
          sInstance = new TraceControl(controllers, initialConfig, listener);
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
  private final AtomicInteger mCurrentTracesMask;
  @Nullable private final TraceControlListener mListener;
  @Nullable private TraceControlHandler mTraceControlHandler;

  // VisibleForTesting
  /*package*/ TraceControl(
      SparseArray<TraceController> controllers,
      @Nullable Config config,
      @Nullable TraceControlListener listener) {
    mControllers = controllers;
    mCurrentConfig = new AtomicReference<>(config);
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
   * Returns lowest available bit from the supplied bit mask
   *
   * @param bitmask bit mask to check
   * @param maxBits Maximum number of bit slots available
   * @return value with lowest free bit set to 1 and 0 if no empty slots left according to maxBits
   */
  private static int findLowestFreeBit(int bitmask, int maxBits) {
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

  public boolean startTrace(int controller, int flags, @Nullable Object context, long longContext) {
    if (findLowestFreeBit(mCurrentTracesMask.get(), MAX_TRACES) == 0) {
      // Reached max traces
      return false;
    }

    TraceController traceController = mControllers.get(controller);
    if (traceController == null) {
      throw new IllegalArgumentException("Unregistered controller for id = " + controller);
    }

    Config rootConfig = mCurrentConfig.get();
    if (rootConfig == null) {
      return false;
    }

    ControllerConfig controllerConfig = null;
    if (traceController.isConfigurable()) {
      controllerConfig = rootConfig.getControllersConfig().getConfigForController(controller);
      if (controllerConfig == null) {
        // We need a config but don't have one
        return false;
      }
    }

    TraceContext traceContext = findCurrentTraceByContext(controller, longContext, context);
    if (traceContext != null) {
      // Attempted start during a trace with the same Id
      return false;
    }

    int providers = traceController.evaluateConfig(longContext, context, controllerConfig);
    if (providers == 0) {
      return false;
    }

    long traceId = nextTraceID();
    Log.w(LOG_TAG, "START PROFILO_TRACEID: " + FbTraceId.encode(traceId));
    TraceContext nextContext =
        new TraceContext(
            traceId,
            FbTraceId.encode(traceId),
            controller,
            traceController,
            context,
            longContext,
            providers,
            traceController.getCpuSamplingRateMs(longContext, context, controllerConfig),
            flags);

    return startTraceInternal(flags, nextContext);
  }

  public boolean adoptContext(int controller, int flags, TraceContext traceContext) {
    TraceController traceController = mControllers.get(controller);
    if (traceController == null) {
      throw new IllegalArgumentException("Unregistered controller for id = " + controller);
    }

    return startTraceInternal(flags, new TraceContext(
        traceContext,
        controller,
        traceController));
  }

  private boolean startTraceInternal(int flags, TraceContext nextContext) {
    while (true) { // Store new trace in CAS manner
      int currentMask = mCurrentTracesMask.get();
      int freeBit = findLowestFreeBit(currentMask, MAX_TRACES);
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

    Config rootConfig = mCurrentConfig.get();
    if (rootConfig == null) {
      return false;
    }
    int timeout = rootConfig.getControllersConfig().getTraceTimeoutMs();
    if (timeout == -1) { // config doesn't specify a value, use default
      timeout = TRACE_TIMEOUT_MS;
    }
    if ((flags & (Trace.FLAG_MANUAL | Trace.FLAG_MEMORY_ONLY)) != 0) {
      // manual and in-memory traces should not time out
      timeout = Integer.MAX_VALUE;
    }
    Logger.postCreateTrace(nextContext.traceId, flags, timeout);

    synchronized (this) {
      ensureHandlerInitialized();
      mTraceControlHandler.onTraceStart(nextContext, timeout);
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
   * Adds timeout entry to the current trace.
   * It is supposed to be called only from {@link LoggerWorkerThread}, not from "frontend" classes.
   */
  void timeoutTrace(long traceId) {
    TraceContext traceContext = findCurrentTraceByTraceId(traceId);
    if (traceContext == null) {
      return;
    }
    Logger.postTimeoutTrace(traceId);
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
          Logger.postAbortTrace(traceContext.traceId);
          mTraceControlHandler.onTraceAbort(new TraceContext(traceContext, abortReason));
          break;
        case TraceStopReason.STOP:
          mTraceControlHandler.onTraceStop(traceContext);
          break;
      }
    }
    return true;
  }

  /**
   * Internal method to ensure consistency for traces we know are aborted in the
   * logger thread through a path that didn't go through us (e.g., timeout).
   *
   * This does not write an abort trace event into the trace.
   *
   * @param id  numeric trace id
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

  @Nullable
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

  /**
   * @return current controller config for the specified controller.
   */
  @Nullable
  public ControllerConfig getControllerConfig(int controller) {
    Config config = mCurrentConfig.get();
    if (config == null) {
      return null;
    }
    return config.getControllersConfig().getConfigForController(controller);
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
    while ((l = Math.abs(sTraceIdRandom.get().nextLong())) <= 0);
    return l;
  }

  private void ensureHandlerInitialized() {
    if (mTraceControlHandler == null) {
      mTraceControlHandler =
          new TraceControlHandler(mListener, TraceControlThreadHolder.getInstance().getLooper());
    }
  }
}
