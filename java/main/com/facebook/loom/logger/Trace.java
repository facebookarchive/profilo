// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.logger;

import com.facebook.loom.core.LoomConstants;
import java.io.File;
import javax.annotation.concurrent.GuardedBy;

public final class Trace {
  // Configuration flags
  public static final int FLAG_MANUAL = 1;
  public static final int FLAG_MEMORY_ONLY = 1 << 1;

  private long mID;
  private final File mLogFile;
  private final int mFlags;
  @GuardedBy("this") private int mAbortReason;

  public Trace(
      long traceId,
      int flags,
      File logFile) {

    mID = traceId;
    mFlags = flags;
    mLogFile = logFile;
    mAbortReason = LoomConstants.ABORT_REASON_UNKNOWN;
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

  public synchronized void setAborted(
      int abortReason) {
    mAbortReason = abortReason;
  }

  public synchronized boolean isAborted() {
    return mAbortReason != LoomConstants.ABORT_REASON_UNKNOWN;
  }

  public synchronized int getAbortReason() {
    return mAbortReason;
  }
}
