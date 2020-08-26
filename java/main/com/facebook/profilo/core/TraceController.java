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

import com.facebook.profilo.config.Config;
import com.facebook.profilo.ipc.TraceConfigExtras;
import javax.annotation.Nullable;

/**
 * TraceController serves as a bridge between TraceControl and the config. The purpose of a
 * TraceController implementation is to provide interpretation to the generic context
 * numbers/Objects and look them up inside a config.
 *
 * <p>There are two types of TraceControllers - configurable or non-configurable (as defined by the
 * return value from isConfigurable).
 *
 * <p>Configurable controllers must implement findTraceConfigIdx - a lookup from context pieces to
 * trace config index inside a Config.
 *
 * <p>Non-configurable controllers must implement getNonConfigurableProviders and
 * getNonConfigurableTraceConfigExtras to provide the same amount of information.
 *
 * <p>contextsEqual exists to determine if the contexts passed to a stopTrace call are the same as
 * the contexts passed to a startTrace call.
 */
public abstract class TraceController {

  public static int RESULT_NO_TRACE_CONFIG = -100;
  public static int RESULT_COINFLIP_MISS = -101;

  /** Returns true if the controller can respond to a config. */
  public abstract boolean isConfigurable();

  // Configurable controller API:

  /**
   * Determine if there is a trace config that matches these context pieces.
   *
   * @param longContext - numeric context piece
   * @param context - Object context piece
   * @param config - Config to look up inside
   * @return the trace config index inside `config` or a RESULT_* code.
   */
  public int findTraceConfigIdx(long longContext, @Nullable Object context, Config config) {
    return RESULT_NO_TRACE_CONFIG;
  }

  /**
   * Determine if the first set of context pieces is the same as the seconds set of context pieces
   */
  public abstract boolean contextsEqual(
      long fstLong, @Nullable Object fst, long sndLong, @Nullable Object snd);

  // Non-configurable controller API:

  /**
   * Determine the providers for a non-configurable controller.
   *
   * @return 0 if a trace is not allowed, a non-0 mask of PROVIDER_ constants otherwise. These
   *     providers will be enabled for the duration of the trace.
   */
  public int getNonConfigurableProviders(long longContext, @Nullable Object context) {
    return 0;
  }

  /** Determine extra parameters for a non-configurable controller */
  public TraceConfigExtras getNonConfigurableTraceConfigExtras(
      long longContext, @Nullable Object context) {
    return TraceConfigExtras.EMPTY;
  }
}
