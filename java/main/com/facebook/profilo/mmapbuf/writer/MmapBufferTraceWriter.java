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
package com.facebook.profilo.mmapbuf.writer;

import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.profilo.writer.NativeTraceWriterCallbacks;
import com.facebook.soloader.SoLoader;

/** Performs conversion of trace buffer file into the regular trace file format. */
@DoNotStrip
public class MmapBufferTraceWriter {
  static {
    SoLoader.loadLibrary("profilo_mmap_file_writer");
  }

  private final NativeTraceWriterCallbacks mCallbacks;
  @DoNotStrip private final HybridData mHybridData;

  public MmapBufferTraceWriter(
      String traceFolder, String tracePrefix, NativeTraceWriterCallbacks callbacks) {
    mCallbacks = callbacks;
    mHybridData = initHybrid(traceFolder, tracePrefix, mCallbacks);
  }

  @DoNotStrip
  private static native HybridData initHybrid(
      String traceFolder, String tracePrefix, NativeTraceWriterCallbacks mCallbacks);

  @DoNotStrip
  public native void nativeWriteTrace(String dumpPath, int qplMarkerId);
}
