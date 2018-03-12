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

package com.facebook.profilo.provider.atrace;

import android.os.Trace;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@DoNotStrip
public final class Atrace {
  static {
    SoLoader.loadLibrary("profilo_atrace");
  }

  private static boolean sHasHook = false;
  private static boolean sHookFailed = false;

  public static synchronized boolean hasHacks() {
    if (!sHasHook && !sHookFailed) {
      sHasHook = installSystraceHook(SystraceProvider.PROVIDER_ATRACE);

      // Record that we failed, so we don't try again.
      sHookFailed = !sHasHook;
    }
    return sHasHook;
  }

  private static native boolean installSystraceHook(int mask);

  public static void enableSystrace() {
    if (!hasHacks()) {
      return;
    }

    enableSystraceNative();

    SystraceReflector.updateSystraceTags();
  }

  public static void restoreSystrace() {
    if (!hasHacks()) {
      return;
    }

    restoreSystraceNative();

    SystraceReflector.updateSystraceTags();
  }

  private static native void enableSystraceNative();
  private static native void restoreSystraceNative();

  private static final class SystraceReflector {
    public static final void updateSystraceTags() {
      if (sTrace_sEnabledTags != null && sTrace_nativeGetEnabledTags != null) {
        try {
          sTrace_sEnabledTags.set(null, sTrace_nativeGetEnabledTags.invoke(null));
        } catch (IllegalAccessException | InvocationTargetException e) {
        }
      }
    }

    private static final Method sTrace_nativeGetEnabledTags;
    private static final Field sTrace_sEnabledTags;
    static {
      Method m;
      try {
        m = Trace.class.getDeclaredMethod("nativeGetEnabledTags");
        m.setAccessible(true);
      } catch (NoSuchMethodException e) {
        m = null;
      }
      sTrace_nativeGetEnabledTags = m;

      Field f;
      try {
        f = Trace.class.getDeclaredField("sEnabledTags");
        f.setAccessible(true);
      } catch (NoSuchFieldException e) {
        f = null;
      }
      sTrace_sEnabledTags = f;
    }
  }
}
