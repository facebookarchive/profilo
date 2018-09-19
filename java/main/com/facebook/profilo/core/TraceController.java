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

import com.facebook.profilo.config.ControllerConfig;
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

  /**
   * Returns true if the controller is based on a config, and false otherwise which means hardcoded
   * behavior.
   */
  boolean isConfigurable();
}
