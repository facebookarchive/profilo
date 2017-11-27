// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.androidinternals.java.lang;

import com.facebook.androidinternals.ReflectionHelper;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/** Provides access to hidden parts of {@link java.lang.Daemons}. */
public final class DaemonsInternal {
  private static boolean sWatchdogStopped = false;

  public static synchronized void stopWatchdog() {
    if (sWatchdogStopped) {
      throw new RuntimeException("Watchdog already stopped.");
    }
    sWatchdogStopped = true;
    try {
      Class sDaemonsClass = Class.forName("java.lang.Daemons");
      Class sDaemonClass = Class.forName("java.lang.Daemons$Daemon");
      Class sWatchdogClass = Class.forName("java.lang.Daemons$FinalizerWatchdogDaemon");

      Field sWatchdogInstanceField = sWatchdogClass.getDeclaredField("INSTANCE");
      sWatchdogInstanceField.setAccessible(true);

      Method sStopMethod = sDaemonClass.getDeclaredMethod("stop");
      sStopMethod.setAccessible(true);

      Object sWatchdogInstance = sWatchdogInstanceField.get(sWatchdogClass);
      sStopMethod.invoke(sWatchdogInstance);
    } catch (InvocationTargetException e) {
      ReflectionHelper.rethrowRuntimeExceptions(e);
    } catch (IllegalAccessException
        | ClassNotFoundException
        | NoSuchMethodException
        | NoSuchFieldException e) {
      throw new RuntimeException(e);
    }
  }
}
