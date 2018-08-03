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

#ifndef ART_RUNTIME_DEX_FILE_TYPES_H_
#define ART_RUNTIME_DEX_FILE_TYPES_H_

#include <museum/8.1.0/external/libcxx/limits>
#include <museum/8.1.0/external/libcxx/ostream>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace dex {

class StringIndex {
 public:
  uint32_t index_;

  constexpr StringIndex() : index_(std::numeric_limits<decltype(index_)>::max()) {}
  explicit constexpr StringIndex(uint32_t idx) : index_(idx) {}

  bool IsValid() const {
    return index_ != std::numeric_limits<decltype(index_)>::max();
  }
  static StringIndex Invalid() {
    return StringIndex(std::numeric_limits<decltype(index_)>::max());
  }

  bool operator==(const StringIndex& other) const {
    return index_ == other.index_;
  }
  bool operator!=(const StringIndex& other) const {
    return index_ != other.index_;
  }
  bool operator<(const StringIndex& other) const {
    return index_ < other.index_;
  }
  bool operator<=(const StringIndex& other) const {
    return index_ <= other.index_;
  }
  bool operator>(const StringIndex& other) const {
    return index_ > other.index_;
  }
  bool operator>=(const StringIndex& other) const {
    return index_ >= other.index_;
  }
};
std::ostream& operator<<(std::ostream& os, const StringIndex& index);

class TypeIndex {
 public:
  uint16_t index_;

  constexpr TypeIndex() : index_(std::numeric_limits<decltype(index_)>::max()) {}
  explicit constexpr TypeIndex(uint16_t idx) : index_(idx) {}

  bool IsValid() const {
    return index_ != std::numeric_limits<decltype(index_)>::max();
  }
  static TypeIndex Invalid() {
    return TypeIndex(std::numeric_limits<decltype(index_)>::max());
  }

  bool operator==(const TypeIndex& other) const {
    return index_ == other.index_;
  }
  bool operator!=(const TypeIndex& other) const {
    return index_ != other.index_;
  }
  bool operator<(const TypeIndex& other) const {
    return index_ < other.index_;
  }
  bool operator<=(const TypeIndex& other) const {
    return index_ <= other.index_;
  }
  bool operator>(const TypeIndex& other) const {
    return index_ > other.index_;
  }
  bool operator>=(const TypeIndex& other) const {
    return index_ >= other.index_;
  }
};
std::ostream& operator<<(std::ostream& os, const TypeIndex& index);

}  // namespace dex
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace std {

template<> struct hash<facebook::museum::MUSEUM_VERSION::art::dex::StringIndex> {
  size_t operator()(const facebook::museum::MUSEUM_VERSION::art::dex::StringIndex& index) const {
    return hash<uint32_t>()(index.index_);
  }
};

template<> struct hash<facebook::museum::MUSEUM_VERSION::art::dex::TypeIndex> {
  size_t operator()(const facebook::museum::MUSEUM_VERSION::art::dex::TypeIndex& index) const {
    return hash<uint16_t>()(index.index_);
  }
};

} } } }  // namespace facebook::museum::MUSEUM_VERSION::std

#endif  // ART_RUNTIME_DEX_FILE_TYPES_H_
