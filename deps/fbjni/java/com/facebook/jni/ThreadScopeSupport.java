// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.jni;

import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;

@DoNotStrip
public class ThreadScopeSupport {
  static {
    SoLoader.loadLibrary("fbjni");
  }

  // This is just used for ThreadScope::withClassLoader to have a java function
  // in the stack so that jni has access to the correct classloader.
  @DoNotStrip
  private static void runStdFunction(long ptr) {
    runStdFunctionImpl(ptr);
  }

  private static native void runStdFunctionImpl(long ptr);
}
