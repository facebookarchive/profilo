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

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import javax.annotation.Nullable;

/** Maintains a one-to-many mapping between entries of a generic type and integer identifiers. */
final class GenericRegistry<EntryType> {

  @javax.annotation.concurrent.GuardedBy("mEntries")
  int sNextEntryBitShift = 0;

  @javax.annotation.concurrent.GuardedBy("mEntries")
  final ArrayList<EntryType> mEntries = new ArrayList<>();

  /**
   * Assigns a new integer identifier to {@code entry}.
   *
   * <p>You can register the same name more than once and receive unique IDs for each call.
   */
  public int newEntry(EntryType entry) {
    synchronized (mEntries) {
      if (sNextEntryBitShift >= 32) {
        throw new IllegalStateException("Attempting to newEntry more than 32 entries.");
      }
      mEntries.add(entry);
      int entryIdx = 1 << sNextEntryBitShift;
      sNextEntryBitShift++;

      return entryIdx;
    }
  }

  /** Retrieve the bitmask for the given entry name. */
  public int getBitMaskFor(EntryType entry) {
    int result = 0;
    synchronized (mEntries) {
      int idx = 0;
      for (EntryType registered : mEntries) {
        if (registered.equals(entry)) {
          result |= 1 << idx;
        }
        idx++;
      }
    }
    return result;
  }

  /** Retrieve the bitmask for the given entries. */
  public int getBitMaskFor(@Nullable Iterable<EntryType> entries) {
    if (entries == null) {
      return 0;
    }
    int result = 0;
    synchronized (mEntries) {
      for (EntryType entry : entries) {
        result |= getBitMaskFor(entry);
      }
    }
    return result;
  }

  /** Retrieve the bitmask for all registered entries */
  public int getBitMaskForAllEntries() {
    return getBitMaskFor(mEntries);
  }

  /** Retrieve all registered entries. */
  public List<EntryType> getRegisteredEntries() {
    synchronized (mEntries) {
      return new ArrayList<>(mEntries);
    }
  }

  /** Retrieve all entries matching the given bitMask */
  public Set<EntryType> getRegisteredEntriesByBitMask(int bitMask) {
    HashSet<EntryType> result = new HashSet<>();
    synchronized (mEntries) {
      int bit = 1;
      for (EntryType entry : mEntries) {
        if ((bit & bitMask) != 0) {
          result.add(entry);
        }
        bit <<= 1;
      }
    }
    return result;
  }
}
