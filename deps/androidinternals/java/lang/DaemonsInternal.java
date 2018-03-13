/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
