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

import java.util.Map;

/**
 * Responsible for managing the currently allowed trace providers.
 */
final public class TraceEvents {

  /**
   * com.facebook.profilo.logger.Logger sets this field to true once it's safe to use native
   * libraries.
   */
  public static boolean sInitialized;

  private static volatile boolean sProviderNamesInitialized;

  public static boolean isEnabled(int provider) {
    return sInitialized && nativeIsEnabled(provider);
  }

  public static int enabledMask(int providers) {
    if (!sInitialized) {
      return 0;
    }
    return nativeEnabledMask(providers);
  }

  public static boolean isProviderNamesInitialized() {
    return sProviderNamesInitialized;
  }

  public static void initProviderNames(Map<String, Integer> providerNamesMap) {
    if (!sInitialized) {
      throw new IllegalStateException("Native library is not initialized.");
    }
    if (sProviderNamesInitialized) {
      return;
    }
    int size = providerNamesMap.size();
    int[] providerIds = new int[size];
    String[] providerNames = new String[size];
    int i = 0;
    for (Map.Entry<String, Integer> nextProvider : providerNamesMap.entrySet()) {
      providerNames[i] = nextProvider.getKey();
      providerIds[i++] = nextProvider.getValue();
    }
    nativeInitProviderNames(providerIds, providerNames);
    sProviderNamesInitialized = true;
  }

  static native boolean nativeIsEnabled(int provider);

  static native int nativeEnabledMask(int providers);

  static native void enableProviders(int providers);

  static native void disableProviders(int providers);

  static native void clearAllProviders();

  static native void nativeInitProviderNames(int[] providerIds, String[] providerNames);
}
