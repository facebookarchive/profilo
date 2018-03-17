/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_EXPERIMENTAL_FLAGS_H_
#define ART_RUNTIME_EXPERIMENTAL_FLAGS_H_

#include <ostream>

namespace art {

// Possible experimental features that might be enabled.
struct ExperimentalFlags {
  // The actual flag values.
  enum {
    kNone           = 0x0000,
    kLambdas        = 0x0001,
  };

  constexpr ExperimentalFlags() : value_(0x0000) {}
  constexpr ExperimentalFlags(decltype(kNone) t) : value_(static_cast<uint32_t>(t)) {}

  constexpr operator decltype(kNone)() const {
    return static_cast<decltype(kNone)>(value_);
  }

  constexpr explicit operator bool() const {
    return value_ != kNone;
  }

  constexpr ExperimentalFlags operator|(const decltype(kNone)& b) const {
    return static_cast<decltype(kNone)>(value_ | static_cast<uint32_t>(b));
  }
  constexpr ExperimentalFlags operator|(const ExperimentalFlags& b) const {
    return static_cast<decltype(kNone)>(value_ | b.value_);
  }

  constexpr ExperimentalFlags operator&(const ExperimentalFlags& b) const {
    return static_cast<decltype(kNone)>(value_ & b.value_);
  }
  constexpr ExperimentalFlags operator&(const decltype(kNone)& b) const {
    return static_cast<decltype(kNone)>(value_ & static_cast<uint32_t>(b));
  }

  constexpr bool operator==(const ExperimentalFlags& b) const {
    return value_ == b.value_;
  }

 private:
  uint32_t value_;
};

inline std::ostream& operator<<(std::ostream& stream, const ExperimentalFlags& e) {
  bool started = false;
  if (e & ExperimentalFlags::kLambdas) {
    stream << (started ? "|" : "") << "kLambdas";
    started = true;
  }
  if (!started) {
    stream << "kNone";
  }
  return stream;
}

inline std::ostream& operator<<(std::ostream& stream, const decltype(ExperimentalFlags::kNone)& e) {
  return stream << ExperimentalFlags(e);
}

}  // namespace art

#endif  // ART_RUNTIME_EXPERIMENTAL_FLAGS_H_
