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

import com.facebook.profilo.core.ProfiloConstants;
import java.io.File;
import javax.annotation.concurrent.GuardedBy;

public final class Trace {
  // Configuration flags
  public static final int FLAG_MANUAL = 1;
  public static final int FLAG_MEMORY_ONLY = 1 << 1;

  private long mID;
  private final File mLogFile;
  private final int mFlags;

  @GuardedBy("this")
  private int mAbortReason;

  public Trace(long traceId, int flags, File logFile) {

    mID = traceId;
    mFlags = flags;
    mLogFile = logFile;
    mAbortReason = ProfiloConstants.ABORT_REASON_UNKNOWN;
  }

  public long getID() {
    return mID;
  }

  public File getLogFile() {
    return mLogFile;
  }

  public int getFlags() {
    return mFlags;
  }

  public synchronized void setAborted(int abortReason) {
    mAbortReason = abortReason;
  }

  public synchronized boolean isAborted() {
    return mAbortReason != ProfiloConstants.ABORT_REASON_UNKNOWN;
  }

  public synchronized int getAbortReason() {
    return mAbortReason;
  }
}
