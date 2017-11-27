/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_DEX_CACHE_RESOLVED_CLASSES_H_
#define ART_RUNTIME_DEX_CACHE_RESOLVED_CLASSES_H_

#include <string>
#include <unordered_set>
#include <vector>

namespace art {

// Data structure for passing around which classes belonging to a dex cache / dex file are resolved.
class DexCacheResolvedClasses {
 public:
  DexCacheResolvedClasses(const std::string& dex_location,
                          const std::string& base_location,
                          uint32_t location_checksum)
      : dex_location_(dex_location),
        base_location_(base_location),
        location_checksum_(location_checksum) {}

  // Only compare the key elements, ignore the resolved classes.
  int Compare(const DexCacheResolvedClasses& other) const {
    if (location_checksum_ != other.location_checksum_) {
      return static_cast<int>(location_checksum_ - other.location_checksum_);
    }
    // Don't need to compare base_location_ since dex_location_ has more info.
    return dex_location_.compare(other.dex_location_);
  }

  template <class InputIt>
  void AddClasses(InputIt begin, InputIt end) const {
    classes_.insert(begin, end);
  }

  const std::string& GetDexLocation() const {
    return dex_location_;
  }

  const std::string& GetBaseLocation() const {
    return base_location_;
  }

  uint32_t GetLocationChecksum() const {
    return location_checksum_;
  }

  const std::unordered_set<uint16_t>& GetClasses() const {
    return classes_;
  }

 private:
  const std::string dex_location_;
  const std::string base_location_;
  const uint32_t location_checksum_;
  // Array of resolved class def indexes.
  mutable std::unordered_set<uint16_t> classes_;
};

inline bool operator<(const DexCacheResolvedClasses& a, const DexCacheResolvedClasses& b) {
  return a.Compare(b) < 0;
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_CACHE_RESOLVED_CLASSES_H_
