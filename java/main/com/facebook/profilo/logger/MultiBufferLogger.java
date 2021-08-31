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

import com.facebook.jni.HybridClassBase;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.proguard.annotations.DoNotStripAny;
import com.facebook.soloader.SoLoader;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * MultiBufferLogger allows entries to be written to one or more buffers at the same time.
 *
 * <p>It's important to note that while the Java-side API relies on BufferLogger for the actual
 * writes, there's a complementary C++ API as well.
 */
@DoNotStripAny
public final class MultiBufferLogger extends HybridClassBase {

  private volatile boolean mLoaded;
  private final AtomicInteger mBuffersCount;
  private volatile long mNativePtr;

  public MultiBufferLogger() {
    mBuffersCount = new AtomicInteger(0);
  }

  private void ensureLoaded() {
    boolean loaded = mLoaded;
    if (loaded) {
      return;
    }

    synchronized (this) {
      loaded = mLoaded;
      if (loaded) {
        return;
      }

      SoLoader.loadLibrary("profilo");
      mNativePtr = initHybrid();
      mLoaded = true;
    }
  }

  public void addBuffer(Buffer buffer) {
    ensureLoaded();
    nativeAddBuffer(buffer);
    mBuffersCount.incrementAndGet();
  }

  public void removeBuffer(Buffer buffer) {
    ensureLoaded();
    nativeRemoveBuffer(buffer);
    mBuffersCount.decrementAndGet();
  }

  public int writeStandardEntry(
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */) {
    if (mBuffersCount.intValue() == 0) {
      return ProfiloConstants.NO_MATCH;
    }
    ensureLoaded();
    return nativeWriteStandardEntry(mNativePtr, flags, type, timestamp, tid, arg1, arg2, arg3);
  }

  public int writeBytesEntry(int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */) {
    if (mBuffersCount.intValue() == 0) {
      return ProfiloConstants.NO_MATCH;
    }
    ensureLoaded();
    return nativeWriteBytesEntry(mNativePtr, flags, type, arg1, arg2);
  }

  private static native int nativeWriteStandardEntry(
      long ptr,
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */);

  private static native int nativeWriteBytesEntry(
      long ptr, int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */);

  private native long initHybrid();

  private native void nativeAddBuffer(Buffer buffer);

  private native void nativeRemoveBuffer(Buffer buffer);
}
