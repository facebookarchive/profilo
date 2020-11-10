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

import com.facebook.profilo.ipc.TraceContext;
import javax.annotation.Nullable;
import javax.annotation.concurrent.NotThreadSafe;

@NotThreadSafe
public class DurationCondition {
  private int[] mReferences;
  private int[] mUploadRates;
  private long mLargestDurationMs = -1;

  public static @Nullable DurationCondition factory(TraceContext context) {
    int[] arr =
        context.mTraceConfigExtras.getIntArrayParam(
            ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION);
    if (arr != null && arr.length > 0) {
      // We have a condition for this config. Add it to the list of integer conditions
      return new DurationCondition(arr);
    }
    return null;
  }

  private DurationCondition(int[] parameters) {
    if ((parameters.length % 2) != 0) {
      throw new IllegalArgumentException("Int conditions should come in pairs");
    }
    mReferences = new int[parameters.length / 2];
    mUploadRates = new int[parameters.length / 2];
    int param_iter = 0;
    for (int insert_idx = 0; insert_idx < mReferences.length; insert_idx++) {
      mReferences[insert_idx] = parameters[param_iter++];
      mUploadRates[insert_idx] = parameters[param_iter++];
      if (mReferences[insert_idx] < 0 || mUploadRates[insert_idx] < 0) {
        throw new IllegalArgumentException("Int conditions should be > 0");
      }
    }
  }

  public void processDurationEvent(long duration) {
    if (duration > mLargestDurationMs) {
      mLargestDurationMs = duration;
    }
  }

  // Return the sample rate from the greatest duration we've seen, without
  // assuming any order in the config
  public int getUploadSampleRate() {
    int maxRef = -1;
    int maxUploadRate = ProfiloConstants.UPLOAD_SAMPLE_RATE_NEVER;
    for (int i = 0; i < mReferences.length; i++) {
      if (mLargestDurationMs > mReferences[i] && mReferences[i] > maxRef) {
        maxRef = mReferences[i];
        maxUploadRate = mUploadRates[i];
      }
    }
    return maxUploadRate;
  }
}
