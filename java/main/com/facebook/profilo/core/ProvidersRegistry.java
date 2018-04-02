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

import com.facebook.proguard.annotations.DoNotStrip;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Maintains a one-to-many mapping between strings and integer identifiers.
 *
 * <p>A config will refer to providers using their names but at runtime it's more efficient to check
 * for bits in an integer. This is where we store the mapping between the two.
 *
 * <p>The general pattern is:
 *
 * <pre>
 *   class MyProvider extends TraceOrchestrator.TraceProvider {
 *   public static final int PROVIDER_MYPROVIDER = ProviderRegistry.newProvider("myprovider");
 *   ...
 *   }
 * </pre>
 */
@DoNotStrip
public final class ProvidersRegistry {

  @javax.annotation.concurrent.GuardedBy("sProviders")
  static int sNextProviderBitShift = 0;

  @javax.annotation.concurrent.GuardedBy("sProviders")
  static final ArrayList<String> sProviders = new ArrayList<>();

  /**
   * Assigns a new integer identifier to {@code provider}.
   *
   * <p>You can register the same name more than once and receive unique IDs for each call.
   */
  public static int newProvider(String provider) {
    synchronized (sProviders) {
      if (sNextProviderBitShift >= 32) {
        throw new IllegalStateException("Attempting to newProvider more than 32 providers.");
      }
      sProviders.add(provider);
      int providerIdx = 1 << sNextProviderBitShift;
      sNextProviderBitShift++;

      return providerIdx;
    }
  }

  /**
   * Retrieve the bitmask for the given provider name.
   *
   * <p>Called from JNI.
   */
  @DoNotStrip
  public static int getBitMaskFor(String provider) {
    int result = 0;
    synchronized (sProviders) {
      int idx = 0;
      for (String registered : sProviders) {
        if (registered.equals(provider)) {
          result |= 1 << idx;
        }
        idx++;
      }
    }
    return result;
  }

  /** Retrieve the bitmask for the given providers. */
  public static int getBitMaskFor(Iterable<String> providers) {
    int result = 0;
    synchronized (sProviders) {
      for (String provider : providers) {
        result |= getBitMaskFor(provider);
      }
    }
    return result;
  }

  /** Retrieve all registered providers. */
  public static List<String> getRegisteredProviders() {
    return Collections.unmodifiableList(sProviders);
  }
}
