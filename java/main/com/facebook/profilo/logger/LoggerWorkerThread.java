// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.logger;

import android.os.Process;
import com.facebook.profilo.writer.NativeTraceWriter;

class LoggerWorkerThread extends Thread {

  private final NativeTraceWriter mTraceWriter;

  LoggerWorkerThread(NativeTraceWriter writer) {
    super("dextr-worker");
    mTraceWriter = writer;
  }

  public void run() {
    Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND - 5);

    mTraceWriter.loop();
  }
}
