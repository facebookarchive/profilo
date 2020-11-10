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

// This class represents a condition to upload a particular trace. Said trace
// could have been configured to have multiple conditions (for instance,
// duration + annotation). Since it's hard to handle them all generically
// (i.e, via some interface), and because we might want to process all conditions
// a bit smarter than just iterating through all of them, we have this class
// which handles multiple conditions and how they interact with each other.
//
// In the end, an object of this class should simply return an upload sampling
// rate which will determine whether the trace is uploaded or not.

@NotThreadSafe
public class TraceCondition {

  private boolean mHasConditions = false;
  private boolean mConditionMalformed = false;
  private @Nullable DurationCondition mDurationCondition = null;
  private @Nullable AnnotationCondition mAnnotationCondition = null;

  public TraceCondition(TraceContext context) {
    // Get the trace config extras from the context and parse all available
    // conditions
    try {
      mDurationCondition = DurationCondition.factory(context);
      mAnnotationCondition = AnnotationCondition.factory(context);
    } catch (IllegalArgumentException e) {
      // Some condition is malformed
      mConditionMalformed = true;
    }
    if (mDurationCondition != null || mAnnotationCondition != null) {
      mHasConditions = true;
    }
  }

  public boolean hasConditions() {
    return mHasConditions;
  }

  public boolean isConditionMalformed() {
    return mConditionMalformed;
  }

  public void processDurationEvent(long duration) {
    if (mDurationCondition == null) {
      return;
    }
    mDurationCondition.processDurationEvent(duration);
  }

  public void processAnnotationEvent(String annotation) {
    if (mAnnotationCondition == null) {
      return;
    }
    mAnnotationCondition.processAnnotationEvent(annotation);
  }

  public int getUploadSampleRate() {
    if (mAnnotationCondition != null) {
      int sampleRate = mAnnotationCondition.getUploadSampleRate();

      // The AnnotationCondition class will return either a sampling rate
      // of 1 if the condition was met, or the fallback value if it wasn't.
      // Thus, we'll use != 1 to indicate "condition wasn't met" here.
      if (sampleRate != ProfiloConstants.UPLOAD_SAMPLE_RATE_ALWAYS) {
        return sampleRate;
      }
      // AnnotationCondition met: fall through to DurationCondition
    }

    if (mDurationCondition != null) {
      return mDurationCondition.getUploadSampleRate();
    }

    // No condition specified. Just upload the trace.
    return ProfiloConstants.UPLOAD_SAMPLE_RATE_ALWAYS;
  }
}
