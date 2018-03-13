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
