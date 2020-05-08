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
package com.facebook.profilo.mmapbuf.reader;

import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

/** Reads trace ID from the buffer file header (if present) */
@DoNotStrip
public class MmapBufferHeaderReader {
  static {
    SoLoader.loadLibrary("profilo_mmapbuf_rdr");
  }

  @DoNotStrip private final HybridData mHybridData;

  public MmapBufferHeaderReader() {
    mHybridData = initHybrid();
  }

  @DoNotStrip
  private static native HybridData initHybrid();

  /**
   * Reads trace ID from buffer file header
   *
   * @param bufferPath - buffer file path
   * @return trace ID of the last running trace, 0 - if not trace was running
   */
  @DoNotStrip
  public native long readTraceId(String bufferPath);
}
