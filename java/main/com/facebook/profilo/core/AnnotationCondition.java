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
import java.util.HashSet;
import javax.annotation.Nullable;
import javax.annotation.concurrent.NotThreadSafe;

@NotThreadSafe
public class AnnotationCondition {

  private interface StringOp {
    public boolean eval(HashSet<String> seenAnnotations, String[] userDefinedValues);
  }

  private static class StringOpAny implements StringOp {
    @Override
    public boolean eval(HashSet<String> seenAnnotations, String[] userDefinedValues) {
      for (String singleValue : userDefinedValues) {
        if (seenAnnotations.contains(singleValue)) {
          return true;
        }
      }
      return false;
    }
  }

  private static class StringOpAll implements StringOp {
    @Override
    public boolean eval(HashSet<String> seenAnnotations, String[] userDefinedValues) {
      for (String singleValue : userDefinedValues) {
        if (!seenAnnotations.contains(singleValue)) {
          return false;
        }
      }
      return true;
    }
  }

  private StringOp mOperator;
  private String[] mValues;
  private HashSet<String> mSeenAnnotations;
  private int mFallbackSamplingRate = 0;

  public static @Nullable AnnotationCondition factory(TraceContext context) {
    String[] arr =
        context.mTraceConfigExtras.getStringArrayParam(
            ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION);
    if (arr != null && arr.length > 0) {
      // Let's see if this is even an annotation condition
      if (arr[0].equals("annotation")) {
        int fallbackSamplingRate =
            context.mTraceConfigExtras.getIntParam(
                ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION_FALLBACK_SAMPLING_RATE,
                ProfiloConstants.UPLOAD_SAMPLE_RATE_NEVER);

        if (fallbackSamplingRate == ProfiloConstants.UPLOAD_SAMPLE_RATE_ALWAYS) {
          // Even if the annotation condition isn't met, we'll always upload
          // the trace. This is equivalent to not having any condition enabled.
          return null;
        }
        return new AnnotationCondition(arr, fallbackSamplingRate);
      }
    }
    return null;
  }

  private AnnotationCondition(String[] parameters, int fallbackSamplingRate) {
    if (parameters.length < 3) {
      throw new IllegalArgumentException("Annotation conditions should have at least 3 elements");
    }

    if (fallbackSamplingRate < 0) {
      throw new IllegalArgumentException("Fallback sampling rate < 0: " + fallbackSamplingRate);
    }

    String op = parameters[1];
    if (op.equals("any")) {
      mOperator = new StringOpAny();
    } else if (op.equals("all")) {
      mOperator = new StringOpAll();
    } else {
      throw new IllegalArgumentException("'" + op + "' is not a valid operation");
    }

    mValues = new String[parameters.length - 2];
    for (int i = 0; i < parameters.length - 2; i++) {
      mValues[i] = parameters[i + 2];
    }

    mSeenAnnotations = new HashSet<>();
    mFallbackSamplingRate = fallbackSamplingRate;
  }

  public void processAnnotationEvent(String annotation) {
    synchronized (mSeenAnnotations) {
      mSeenAnnotations.add(annotation);
    }
  }

  public int getUploadSampleRate() {
    synchronized (mSeenAnnotations) {
      if (mOperator.eval(mSeenAnnotations, mValues)) {
        return 1;
      }
    }
    return mFallbackSamplingRate;
  }
}
