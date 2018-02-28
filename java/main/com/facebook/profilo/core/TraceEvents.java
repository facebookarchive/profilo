// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.core;

/**
 * Responsible for managing the currently allowed trace providers.
 */
final public class TraceEvents {

  /**
   * com.facebook.profilo.logger.Logger sets this field to true once it's safe to use native
   * libraries.
   */
  public static boolean sInitialized;

  public static boolean isEnabled(int provider) {
    return sInitialized && nativeIsEnabled(provider);
  }

  public static int enabledMask(int providers) {
    if (!sInitialized) {
      return 0;
    }
    return nativeEnabledMask(providers);
  }

  static native boolean nativeIsEnabled(int provider);

  static native int nativeEnabledMask(int providers);

  static native void enableProviders(int providers);

  static native void disableProviders(int providers);

  static native void clearAllProviders();
}
