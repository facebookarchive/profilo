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

import com.facebook.proguard.annotations.DoNotStrip;
import java.util.List;
import java.util.Set;
import javax.annotation.Nullable;

/**
 * Maintains a one-to-many mapping between strings and integer identifiers.
 *
 * <p>A config will refer to providers using their names but at runtime it's more efficient to check
 * for bits in an integer. This is where we store the mapping between the two.
 *
 * <p>The general pattern is:
 *
 * <pre>
 *   class MyProvider extends BaseTraceProvider {
 *   public static final int PROVIDER_MYPROVIDER = ProviderRegistry.newProvider("myprovider");
 *   ...
 *   }
 * </pre>
 */
@DoNotStrip
public final class ProvidersRegistry {
  static final GenericRegistry<String> sRegistry = new GenericRegistry<>();

  /**
   * Assigns a new integer identifier to {@code provider}.
   *
   * <p>You can register the same name more than once and receive unique IDs for each call.
   */
  public static int newProvider(String provider) {
    return sRegistry.newEntry(provider);
  }

  /**
   * Retrieve the bitmask for the given provider name.
   *
   * <p>Called from JNI.
   */
  @DoNotStrip
  public static int getBitMaskFor(String provider) {
    return sRegistry.getBitMaskFor(provider);
  }

  /** Retrieve the bitmask for the given providers. */
  public static int getBitMaskFor(@Nullable Iterable<String> providers) {
    return sRegistry.getBitMaskFor(providers);
  }

  /** Retrieve the list of the registered provider names. */
  public static List<String> getRegisteredProviders() {
    return sRegistry.getRegisteredEntries();
  }

  public static Set<String> getRegisteredProvidersByBitMask(int bitMask) {
    return sRegistry.getRegisteredEntriesByBitMask(bitMask);
  }

  public static int getBitmaskForAllEntries() {
    return sRegistry.getBitMaskForAllEntries();
  }
}
