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
import com.facebook.profilo.mmapbuf.Buffer;
import com.facebook.proguard.annotations.DoNotStripAny;
import com.facebook.soloader.SoLoader;

/**
 * MultiBufferLogger allows entries to be written to one or more buffers at the same time.
 *
 * <p>It's important to note that while the Java-side API relies on BufferLogger for the actual
 * writes, there's a complementary C++ API as well.
 */
@DoNotStripAny
public final class MultiBufferLogger extends HybridClassBase {

  private boolean mLoaded;

  public MultiBufferLogger() {}

  private void ensureLoaded() {
    if (mLoaded) {
      return;
    }

    synchronized (this) {
      if (mLoaded) {
        return;
      }

      SoLoader.loadLibrary("profilo");
      initHybrid();
      mLoaded = true;
    }
  }

  public void addBuffer(Buffer buffer) {
    ensureLoaded();
    nativeAddBuffer(buffer);
  }

  public void removeBuffer(Buffer buffer) {
    ensureLoaded();
    nativeRemoveBuffer(buffer);
  }

  public int writeStandardEntry(
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */) {
    ensureLoaded();
    return nativeWriteStandardEntry(flags, type, timestamp, tid, arg1, arg2, arg3);
  }

  public int writeBytesEntry(int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */) {
    ensureLoaded();
    return nativeWriteBytesEntry(flags, type, arg1, arg2);
  }

  private native int nativeWriteStandardEntry(
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */);

  private native int nativeWriteBytesEntry(
      int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */);

  private native void initHybrid();

  private native void nativeAddBuffer(Buffer buffer);

  private native void nativeRemoveBuffer(Buffer buffer);
}
