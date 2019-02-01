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

package com.facebook.androidinternals.android.os;

import android.os.Build;
import com.facebook.androidinternals.ReflectionHelper;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import javax.annotation.Nullable;

public final class SystemPropertiesInternal {

  /** android.os.SystemProperties#get(String) */
  private static final @Nullable Method sGetEmptyDefault;

  /** android.os.SystemProperties#getLong(String, long) */
  private static final @Nullable Method sGetLong;

  /** android.os.SystemProperties#set(String, String) */
  private static final @Nullable Method sSet;

  /** android.os.SystemProperties#addChangeCallback(Runnable) */
  private static final @Nullable Method sAddChangeCallback;

  private static volatile boolean sHiddenMembersEnabled;

  private static class SystemPropertiesHiddenMembers {
    public final @Nullable Method addChangeCallback;
    public final Method getEmptyDefault;
    public final Method getLong;
    public final Method set;

    public static @Nullable SystemPropertiesHiddenMembers newInstance() {
      try {
        final Class systemProperties = Class.forName("android.os.SystemProperties");

        final Method getEmptyDefault = systemProperties.getMethod("get", String.class);
        final Method getLong = systemProperties.getMethod("getLong", String.class, long.class);
        final Method set = systemProperties.getMethod("set", String.class, String.class);
        final Method addChangeCallback =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN
                ? systemProperties.getMethod("addChangeCallback", Runnable.class)
                : null;

        return new SystemPropertiesHiddenMembers(addChangeCallback, getEmptyDefault, getLong, set);
      } catch (ClassNotFoundException e) {
        return null;
      } catch (NoSuchMethodException e) {
        return null;
      }
    }

    private SystemPropertiesHiddenMembers(
        final @Nullable Method addChangeCallback,
        final Method getEmptyDefault,
        final Method getLong,
        final Method set) {
      this.addChangeCallback = addChangeCallback;
      this.getEmptyDefault = getEmptyDefault;
      this.getLong = getLong;
      this.set = set;
    }
  }

  static {
    final SystemPropertiesHiddenMembers hiddenMembers = SystemPropertiesHiddenMembers.newInstance();

    if (hiddenMembers != null) {
      sAddChangeCallback = hiddenMembers.addChangeCallback;
      sGetEmptyDefault = hiddenMembers.getEmptyDefault;
      sGetLong = hiddenMembers.getLong;
      sSet = hiddenMembers.set;
      sHiddenMembersEnabled = true;
    } else {
      sAddChangeCallback = null;
      sGetEmptyDefault = null;
      sGetLong = null;
      sSet = null;
      sHiddenMembersEnabled = false;
    }
  }

  private SystemPropertiesInternal() {}

  public static String get(final String key) {
    if (!sHiddenMembersEnabled) {
      return "";
    }

    final String result = (String) invoke(sGetEmptyDefault, key);
    if (result == null) {
      return "";
    }

    return result;
  }

  public static String get(final String key, String def) {
    final String result = get(key);
    return result != null && !result.isEmpty() ? result : def;
  }

  public static long getLong(final String key, final long def) {
    if (!sHiddenMembersEnabled) {
      return def;
    }

    final Long result = (Long) invoke(sGetLong, key, def);
    if (result == null) {
      return def;
    }

    return result;
  }

  public static void set(final String key, final String value) {
    if (!sHiddenMembersEnabled) {
      return;
    }

    invoke(sSet, key, value);
  }

  public static void addChangeCallback(final Runnable callback) {
    if (!sHiddenMembersEnabled) {
      return;
    }

    invoke(sAddChangeCallback, callback);
  }

  @Nullable
  private static Object invoke(final @Nullable Method method, final Object... args) {
    if (method == null) {
      return null;
    }

    try {
      return method.invoke(null, (Object[]) args);
    } catch (IllegalAccessException e) {
      // This shouldn't happen, but if it does something is horribly wrong. Fall back to assuming
      // we have no Trace support.
      sHiddenMembersEnabled = false;

      return null;
    } catch (InvocationTargetException e) {
      ReflectionHelper.rethrowRuntimeExceptions(e);

      // The method signature of all methods in the Trace class do not allow them to throw any
      // other kind of exception, so we should never get here
      return null; // Keep the compiler happy
    }
  }
}
