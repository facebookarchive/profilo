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
 *   class MyController implements TraceController {
 *   public static final int TRIGGER_MYTRIGGER = TriggerRegistry.newTrigger("mytrigger");
 *   ...
 *   }
 * </pre>
 */
@DoNotStrip
public final class TriggerRegistry {
  static final GenericRegistry<String> sRegistry = new GenericRegistry<>();

  /**
   * Assigns a new integer identifier to {@code trigger}.
   *
   * <p>You can register the same name more than once and receive unique IDs for each call.
   */
  public static int newTrigger(String trigger) {
    return sRegistry.newEntry(trigger);
  }

  /** Retrieve the bitmask for the given trigger name. */
  @DoNotStrip
  public static int getBitMaskFor(String trigger) {
    return sRegistry.getBitMaskFor(trigger);
  }

  /** Retrieve the bitmask for the given triggers. */
  public static int getBitMaskFor(@Nullable Iterable<String> triggers) {
    return sRegistry.getBitMaskFor(triggers);
  }

  /** Retrieve the list of the registered trigger names. */
  public static List<String> getRegisteredTriggers() {
    return sRegistry.getRegisteredEntries();
  }

  /** Retrieve the bitmask for the registered triggers. */
  public static int getBitMaskForAllTriggers() {
    return sRegistry.getBitMaskForAllEntries();
  }

  /** Retrieve the names of the triggers from their bit mask. */
  public static Set<String> getRegisteredTriggersByBitMask(int bitMask) {
    return sRegistry.getRegisteredEntriesByBitMask(bitMask);
  }
}
