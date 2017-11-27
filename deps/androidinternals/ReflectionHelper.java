// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.androidinternals;

import java.lang.reflect.InvocationTargetException;

public final class ReflectionHelper {
  private ReflectionHelper() {}

  /** Could use Guava, but this is in the startup path and we want to keep the primary dex small. */
  public static void rethrowRuntimeExceptions(final InvocationTargetException e) {
    final Throwable targetException = e.getTargetException();
    if (targetException instanceof RuntimeException) {
      throw (RuntimeException) targetException;
    } else if (targetException instanceof Error) {
      throw (Error) targetException;
    }
  }
}
