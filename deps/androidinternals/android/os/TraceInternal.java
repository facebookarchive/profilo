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

import static com.facebook.infer.annotation.Assertions.assertNotNull;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Trace;
import com.facebook.androidinternals.ReflectionHelper;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import javax.annotation.Nullable;

/**
 * Provides access to hidden members on {@link android.os.Trace}. This class should generally not be
 * used directly; {@link com.facebook.systrace.Systrace} wraps this class and provides additional
 * functionality.
 */
public final class TraceInternal {
  public static final boolean ANDROID_OS_TRACE_SUPPORTED =
      Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2;

  /** android.os.Trace.isTagEnabled(long traceTag) */
  private static final @Nullable Method sIsTagEnabled;
  /** android.os.Trace.setAppTracingAllowed(boolean allowed) */
  private static final @Nullable Method sSetAppTracingAllowed;
  /**
   * android.os.Trace.TRACE_TAG_APP. We reflect to get it because technically it can change from
   * release to release
   */
  public static final long TRACE_TAG_APP;
  /**
   * android.os.trace.MAX_SECTION_NAME_LEN. We can't reflect to get it because it's private, but it
   * is unlikely to get smaller over time.
   */
  public static final int MAX_SECTION_NAME_LEN = 127;

  /**
   * Controls whether we use hidden members of android.os.Trace at all. Other Internals classes may
   * make one method of a class accessible even if others fail, but due to the nature of tracing we
   * prefer this class to behave in an all-or-nothing manner.
   */
  private static volatile boolean sHiddenMembersEnabled;

  /**
   * Helper class for getting the hidden members of Systrace. This helps us initialize them all at
   * once at class-load time.
   */
  private static class SystraceHiddenMembers {

    public final Method isTagEnabled;
    public final Method setAppTracingAllowed;
    public final long TRACE_TAG_APP;

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @Nullable
    public static SystraceHiddenMembers newInstance() {
      try {
        final Method isTagEnabled = Trace.class.getMethod("isTagEnabled", long.class);
        final Method setAppTracingAllowed =
            Trace.class.getMethod("setAppTracingAllowed", boolean.class);

        final Field appTraceTagField = Trace.class.getField("TRACE_TAG_APP");
        if (appTraceTagField.getType() != long.class) {
          return null;
        }
        final long traceTagApp = appTraceTagField.getLong(null);

        return new SystraceHiddenMembers(isTagEnabled, setAppTracingAllowed, traceTagApp);
      } catch (NoSuchMethodException e) {
        return null;
      } catch (IllegalAccessException e) {
        return null;
      } catch (NoSuchFieldException e) {
        return null;
      }
    }

    public SystraceHiddenMembers(
        final Method isTagEnabled, final Method setAppTracingAllowed, final long traceTagApp) {
      this.isTagEnabled = isTagEnabled;
      this.setAppTracingAllowed = setAppTracingAllowed;
      this.TRACE_TAG_APP = traceTagApp;
    }
  }

  static {
    SystraceHiddenMembers hiddenMembers = null;
    if (ANDROID_OS_TRACE_SUPPORTED) {
      hiddenMembers = SystraceHiddenMembers.newInstance();
    }

    if (hiddenMembers != null) {
      sIsTagEnabled = hiddenMembers.isTagEnabled;
      sSetAppTracingAllowed = hiddenMembers.setAppTracingAllowed;
      TRACE_TAG_APP = hiddenMembers.TRACE_TAG_APP;
      sHiddenMembersEnabled = true;
    } else {
      sIsTagEnabled = null;
      sSetAppTracingAllowed = null;
      TRACE_TAG_APP = 0;
      sHiddenMembersEnabled = false;
    }
  }

  private TraceInternal() {}

  public static boolean isTagEnabled(final long traceTag) {
    if (!sHiddenMembersEnabled) {
      return false;
    }

    final Boolean result = (Boolean) invoke(assertNotNull(sIsTagEnabled), traceTag);
    if (result == null) {
      return false;
    }

    return result;
  }

  public static void setAppTracingAllowed(final boolean allowed) {
    if (!sHiddenMembersEnabled) {
      return;
    }

    invoke(assertNotNull(sSetAppTracingAllowed), allowed);
  }

  private static @Nullable Object invoke(final Method method, final Object... args) {
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
