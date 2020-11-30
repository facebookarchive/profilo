/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.facebook.profilo.core;

import com.facebook.soloader.SoLoader;
import java.util.List;

/** Responsible for managing the currently allowed trace providers. */
public final class TraceEvents {

  static void initialize() {
    SoLoader.loadLibrary("profilo");
  }

  private static int sLastNameRefreshProvidersState;
  private static volatile int sProviders = 0;

  public static boolean isEnabled(int provider) {
    return (sProviders & provider) != 0;
  }

  public static int enabledMask(int providers) {
    return sProviders & providers;
  }

  public static synchronized void enableProviders(int providers) {
    sProviders = nativeEnableProviders(providers);
  }

  public static synchronized void disableProviders(int providers) {
    sProviders = nativeDisableProviders(providers);
  }

  /** Clearing all providers can lead to unpredicted effects in active providers mask state. */
  @Deprecated
  public static synchronized void clearAllProviders() {
    nativeClearAllProviders();
    sProviders = 0;
  }

  public static synchronized void refreshProviderNames() {
    int currentBitmaskForAllEntries = ProvidersRegistry.getBitmaskForAllEntries();
    if (currentBitmaskForAllEntries == sLastNameRefreshProvidersState) {
      return;
    }
    sLastNameRefreshProvidersState = currentBitmaskForAllEntries;

    List<String> registeredProviders = ProvidersRegistry.getRegisteredProviders();
    int size = registeredProviders.size();
    int[] providerIds = new int[size];
    String[] providerNames = new String[size];

    int i = 0;
    for (String nextProvider : registeredProviders) {
      providerNames[i] = nextProvider;
      providerIds[i++] = ProvidersRegistry.getBitMaskFor(nextProvider);
    }

    nativeRefreshProviderNames(providerIds, providerNames);
  }

  static native int nativeEnableProviders(int providers);

  static native int nativeDisableProviders(int providers);

  static native void nativeClearAllProviders();

  static native void nativeRefreshProviderNames(int[] providerIds, String[] providerNames);
}
