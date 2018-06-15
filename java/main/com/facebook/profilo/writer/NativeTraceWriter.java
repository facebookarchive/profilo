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

package com.facebook.profilo.writer;

import com.facebook.jni.HybridData;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

@DoNotStrip
public final class NativeTraceWriter {
  static {
    SoLoader.loadLibrary("profilo");
  }

  @DoNotStrip
  private HybridData mHybridData;

  public NativeTraceWriter(
      String traceFolder,
      String tracePrefix,
      NativeTraceWriterCallbacks callbacks) {
    mHybridData = initHybrid(
      traceFolder,
      tracePrefix,
      callbacks);
  }

  private static native HybridData initHybrid(
      String traceFolder,
      String tracePrefix,
      NativeTraceWriterCallbacks callbacks);

  public native void loop();
  public native String getTraceFolder(long traceID);
}
