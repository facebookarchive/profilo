// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.yarn;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;
import static android.os.Process.setThreadPriority;

import com.facebook.loom.core.LoomConstants;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import javax.annotation.concurrent.GuardedBy;

@DoNotStrip
public class PerfEventsSession {

  static {
    SoLoader.loadLibrary("loom_yarn");
  }

  // Lower than UI (0), higher than BACKGROUND (10)
  private static final int READER_THREAD_PRIORITY = THREAD_PRIORITY_BACKGROUND / 2;

  /**
   * How many times to try to attach to all threads before giving up.
   */
  private static final int MAX_ATTACH_ITERATIONS = 3;

  /**
   * The maximum number of file descriptors the session can use up as a ratio of the maximum
   * number of descriptors allowed for this process.
   */
  private static final float MAX_ATTACHED_FDS_OPEN_RATIO = 0.5f;

  /**
   * If unable to attach due to the max file descriptors limit, attempt to raise it via setrlimit.
   */
  private static final int FALLBACK_RAISE_RLIMIT = 1;

  private final Runnable mSessionRunnable;
  @GuardedBy("this") private Thread mThread;
  @GuardedBy("this") private long mNativeHandle;

  public PerfEventsSession() {
    mSessionRunnable = new Runnable() {
      @Override
      public void run() {
        setThreadPriority(READER_THREAD_PRIORITY);
        nativeStart(mNativeHandle);
      }
    };
  }

  public synchronized boolean attach(int providers) {
    if (mNativeHandle != 0) {
      throw new IllegalStateException("Already attached");
    }
    if ((providers & LoomConstants.PROVIDER_MAJOR_FAULTS) != 0 ||
        (providers & LoomConstants.PROVIDER_THREAD_SCHEDULE) != 0) {
      mNativeHandle = nativeAttach(
          providers,
          FALLBACK_RAISE_RLIMIT,
          MAX_ATTACH_ITERATIONS,
          MAX_ATTACHED_FDS_OPEN_RATIO);
    }
    return mNativeHandle != 0;
  }

  public synchronized void detach() {
    if (mNativeHandle == 0) {
      return; // Nothing to do, not attached.
    }
    nativeDetach(mNativeHandle);
    mNativeHandle = 0;
  }

  /**
   * Starts the session and blocks until it exits. Call {@link #stop()} to end the session.
   */
  public synchronized void start() {
    if (mThread != null) {
      throw new IllegalStateException("Thread is already running");
    }
    Thread thread = new Thread(mSessionRunnable, "YarnWriteToLoomSession");
    thread.start();
    mThread = thread;
  }

  public synchronized void stop() {
    if (mNativeHandle == 0) {
      if (mThread != null) {
        throw new IllegalStateException("Inconsistent state: have thread but no handle");
      }
      return; // we never started, nothing to stop
    }

    nativeStop(mNativeHandle);
    waitForThread();
  }

  @GuardedBy("this")
  private void waitForThread() {
    try {
      mThread.join();
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
    mThread = null;
  }

  @Override
  protected void finalize() throws Throwable {
    stop();
    synchronized (this) {
      if (mNativeHandle != 0) {
        nativeDetach(mNativeHandle);
      }
    }
    super.finalize();
  }

  private static native long nativeAttach(
      int providers,
      int fallbacks,
      int maxAttachIterations,
      float maxAttachedFdsOpenRatio);

  private static native void nativeDetach(long handle);

  private static native void nativeStart(long handle);

  private static native void nativeStop(long handle);
}
