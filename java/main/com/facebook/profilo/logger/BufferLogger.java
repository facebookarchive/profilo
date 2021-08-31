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

import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.profilo.writer.NativeTraceWriter;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

@DoNotStrip
public final class BufferLogger {

  static {
    SoLoader.loadLibrary("profilo");
  }

  public static native int writeStandardEntry(
      Buffer buffer,
      int flags,
      int type,
      long timestamp,
      int tid,
      int arg1 /* callid */,
      int arg2 /* matchid */,
      long arg3 /* extra */);

  public static native int writeBytesEntry(
      Buffer buffer, int flags, int type, int arg1 /* matchid */, String arg2 /* bytes */);

  public static native int writeAndWakeupTraceWriter(
      NativeTraceWriter writer,
      Buffer buffer,
      long traceId,
      int type,
      int callid,
      int matchid,
      long extra);
}
