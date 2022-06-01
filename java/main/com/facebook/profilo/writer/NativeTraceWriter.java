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
package com.facebook.profilo.writer;

import com.facebook.jni.HybridData;
import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import javax.annotation.Nullable;

@DoNotStrip
public final class NativeTraceWriter {
  static {
    SoLoader.loadLibrary("profilo");
  }

  @DoNotStrip private HybridData mHybridData;

  public NativeTraceWriter(
      Buffer buffer,
      String traceFolder,
      String tracePrefix,
      @Nullable NativeTraceWriterCallbacks callbacks) {
    mHybridData = initHybrid(buffer, traceFolder, tracePrefix, callbacks);
  }

  private static native HybridData initHybrid(
      Buffer buffer,
      String traceFolder,
      String tracePrefix,
      @Nullable NativeTraceWriterCallbacks callbacks);

  public native void loop();

  public native void dump(long trace_id);

  public native String getTraceFolder(long traceID);
}
