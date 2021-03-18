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
package com.facebook.profilo.logger;

import com.facebook.profilo.writer.NativeTraceWriterCallbacks;

/**
 * This class optionally delays the onTraceWriteEnd/Abort/Exception callbacks until emit() is
 * called. This is useful when we need to do more work after the onTraceEnd has occurred, like
 * dumping extra buffers.
 */
public class CachingNativeTraceWriterCallbacks implements NativeTraceWriterCallbacks {

  private final boolean mDelayTraceEndsCallback;
  private final NativeTraceWriterCallbacks mDelegate;

  private boolean mSeenAbort;
  private boolean mSeenEnd;
  private boolean mSeenException;
  private long mStoredTraceId;
  private int mStoredAbortReason;
  private Throwable mStoredThrowable;

  public CachingNativeTraceWriterCallbacks(
      boolean delayTraceEndsCallback, NativeTraceWriterCallbacks delegate) {
    mDelayTraceEndsCallback = delayTraceEndsCallback;
    mDelegate = delegate;
  }

  public void emit() {
    if (!mDelayTraceEndsCallback) {
      return;
    }
    if (mSeenException) {
      mDelegate.onTraceWriteException(mStoredTraceId, mStoredThrowable);
      return;
    }
    if (mSeenEnd) {
      mDelegate.onTraceWriteEnd(mStoredTraceId);
      return;
    }
    if (mSeenAbort) {
      mDelegate.onTraceWriteAbort(mStoredTraceId, mStoredAbortReason);
    }
  }

  @Override
  public void onTraceWriteStart(long traceId, int flags) {
    mDelegate.onTraceWriteStart(traceId, flags);
  }

  @Override
  public void onTraceWriteEnd(long traceId) {
    if (mDelayTraceEndsCallback) {
      mSeenEnd = true;
      mStoredTraceId = traceId;
    } else {
      mDelegate.onTraceWriteEnd(traceId);
    }
  }

  @Override
  public void onTraceWriteAbort(long traceId, int abortReason) {
    if (mDelayTraceEndsCallback) {
      mSeenAbort = true;
      mStoredAbortReason = abortReason;
      mStoredTraceId = traceId;
    } else {
      mDelegate.onTraceWriteAbort(traceId, abortReason);
    }
  }

  @Override
  public void onTraceWriteException(long traceId, Throwable t) {
    if (mDelayTraceEndsCallback) {
      mSeenException = true;
      mStoredThrowable = t;
      mStoredTraceId = traceId;
    } else {
      mDelegate.onTraceWriteException(traceId, t);
    }
  }
}
