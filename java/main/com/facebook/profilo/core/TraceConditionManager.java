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

import android.util.LongSparseArray;
import com.facebook.profilo.ipc.TraceContext;
import javax.annotation.Nullable;
import javax.annotation.concurrent.ThreadSafe;

public class TraceConditionManager {

  public static final int MAX_UPLOAD_SAMPLE_RATE = 1;

  @ThreadSafe
  private static class TraceCondition {

    @ThreadSafe
    private static class DurationCondition {
      private int[] mReferences;
      private int[] mUploadRates;
      private long mLargestDurationMs = -1;

      public DurationCondition(int[] parameters) {
        if ((parameters.length % 2) != 0) {
          throw new IllegalArgumentException("Int conditions should come in pairs");
        }
        mReferences = new int[parameters.length / 2];
        mUploadRates = new int[parameters.length / 2];
        int param_iter = 0;
        for (int insert_idx = 0; insert_idx < mReferences.length; insert_idx++) {
          mReferences[insert_idx] = parameters[param_iter++];
          mUploadRates[insert_idx] = parameters[param_iter++];
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
        int maxUploadRate = 0;
        for (int i = 0; i < mReferences.length; i++) {
          if (mLargestDurationMs > mReferences[i] && mReferences[i] > maxRef) {
            maxRef = mReferences[i];
            maxUploadRate = mUploadRates[i];
          }
        }
        return maxUploadRate;
      }
    }

    private boolean mHasConditions = false;
    private @Nullable DurationCondition mDurationCondition = null;

    public TraceCondition(TraceContext context) {
      // Get the trace config extras from the context and parse all available
      // conditions
      int[] arr =
          context.mTraceConfigExtras.getIntArrayParam(
              ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION);
      if (arr != null && arr.length > 0) {
        // We have a condition for this config. Add it to the list of integer conditions
        mDurationCondition = new DurationCondition(arr);
        mHasConditions = true;
      }
    }

    public boolean hasConditions() {
      return mHasConditions;
    }

    public void processDurationEvent(long duration) {
      if (mDurationCondition == null) {
        throw new IllegalStateException("We should have set a duration");
      }
      mDurationCondition.processDurationEvent(duration);
    }

    public int getUploadSampleRate() {
      if (mDurationCondition == null) {
        return MAX_UPLOAD_SAMPLE_RATE;
      }
      return mDurationCondition.getUploadSampleRate();
    }
  }

  // traceId -> TraceCondition
  private LongSparseArray<TraceCondition> mConditions;

  public TraceConditionManager() {
    mConditions = new LongSparseArray<>();
  }

  public void registerTrace(TraceContext context) {
    TraceCondition cond = new TraceCondition(context);
    if (cond.hasConditions()) {
      mConditions.put(context.traceId, cond);
    }
  }

  public void unregisterTrace(TraceContext context) {
    mConditions.delete(context.traceId);
  }

  public void processDurationEvent(long traceId, long duration) {
    TraceCondition cond = mConditions.get(traceId);
    if (cond == null) {
      return;
    }
    cond.processDurationEvent(duration);
  }

  public int getUploadSampleRate(long traceId) {
    TraceCondition cond = mConditions.get(traceId);
    if (cond == null) {
      // No trace conditions for this trace, so upload unconditionally
      return MAX_UPLOAD_SAMPLE_RATE;
    }
    return cond.getUploadSampleRate();
  }
}
