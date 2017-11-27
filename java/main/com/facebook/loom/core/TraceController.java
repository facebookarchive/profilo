// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import com.facebook.loom.config.ControllerConfig;
import javax.annotation.Nullable;

public interface TraceController {

  /**
   * Determine if the configuration allows for a trace to start.
   *
   * @param context the trace context object passed to
   *                {@link TraceControl#startTrace(int, int, Object, int)}
   * @param config the current config for this controller
   * @return 0 if a trace is not allowed, a non-0 mask of PROVIDER_ constants otherwise.
   *         These providers will be enabled for the duration of the trace.
   */
  int evaluateConfig(@Nullable Object context, ControllerConfig config);

  /**
   * Determine current profiler sample rate in milliseconds for a starting trace.
   *
   * @param context the trace context object passed to
   *                {@link TraceControl#startTrace(int, int, Object, int)}
   * @param config the current config for this controller
   * @return sampling rate in milliseconds, 0 if not configured
   */
  int getCpuSamplingRateMs(Object context, ControllerConfig config);

  boolean contextsEqual(int fstInt, @Nullable Object fst, int sndInt, @Nullable Object snd);
}
