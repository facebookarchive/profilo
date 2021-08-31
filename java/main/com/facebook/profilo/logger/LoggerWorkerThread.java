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

import android.os.Process;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.profilo.writer.NativeTraceWriter;
import com.facebook.profilo.writer.NativeTraceWriterCallbacks;

public class LoggerWorkerThread extends Thread {

  private final long mTraceId;
  private final String mFolder;
  private final String mPrefix;
  private final Buffer[] mBuffers;
  private final CachingNativeTraceWriterCallbacks mCallbacks;
  private final NativeTraceWriter mMainTraceWriter;

  public LoggerWorkerThread(
      long traceId,
      String folder,
      String prefix,
      Buffer[] buffers,
      NativeTraceWriterCallbacks callbacks) {
    super("Prflo:Logger");
    mTraceId = traceId;
    mFolder = folder;
    mPrefix = prefix;
    mBuffers = buffers;
    boolean needsCachedCallbacks = buffers.length > 1;
    mCallbacks = new CachingNativeTraceWriterCallbacks(needsCachedCallbacks, callbacks);
    mMainTraceWriter = new NativeTraceWriter(buffers[0], folder, prefix + "-0", mCallbacks);
  }

  public NativeTraceWriter getTraceWriter() {
    return mMainTraceWriter;
  }

  public void run() {
    try {
      Process.setThreadPriority(ProfiloConstants.TRACE_CONFIG_PARAM_LOGGER_PRIORITY_DEFAULT);
      mMainTraceWriter.loop();

      if (mBuffers.length > 1) {
        dumpSecondaryBuffers();
      }
    } catch (RuntimeException ex) {
      mCallbacks.onTraceWriteException(mTraceId, ex);
    } finally {
      mCallbacks.emit();
    }
  }

  private void dumpSecondaryBuffers() {
    StringBuilder prefixBuilder = new StringBuilder(mPrefix.length() + 2);
    for (int idx = 1; idx < mBuffers.length; idx++) {
      prefixBuilder.setLength(0);
      prefixBuilder.append(mPrefix).append('-').append(idx);

      // Additional buffers do not have trace control entries, so will not
      // need to issue callbacks.
      NativeTraceWriter bufferDumpWriter =
          new NativeTraceWriter(mBuffers[idx], mFolder, prefixBuilder.toString(), null);
      bufferDumpWriter.dump(mTraceId);
    }
  }
}
