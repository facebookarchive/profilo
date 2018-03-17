/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_CODE_OFFSET_H_
#define ART_RUNTIME_ARCH_CODE_OFFSET_H_

#include <iosfwd>

#include "base/bit_utils.h"
#include "base/logging.h"
#include "instruction_set.h"

namespace art {

// CodeOffset is a holder for compressed code offsets. Since some architectures have alignment
// requirements it is possible to compress code offsets to reduce stack map sizes.
class CodeOffset {
 public:
  ALWAYS_INLINE static CodeOffset FromOffset(uint32_t offset, InstructionSet isa = kRuntimeISA) {
    return CodeOffset(offset / GetInstructionSetInstructionAlignment(isa));
  }

  ALWAYS_INLINE static CodeOffset FromCompressedOffset(uint32_t offset) {
    return CodeOffset(offset);
  }

  ALWAYS_INLINE uint32_t Uint32Value(InstructionSet isa = kRuntimeISA) const {
    uint32_t decoded = value_ * GetInstructionSetInstructionAlignment(isa);
    DCHECK_GE(decoded, value_) << "Integer overflow";
    return decoded;
  }

  // Return compressed internal value.
  ALWAYS_INLINE uint32_t CompressedValue() const {
    return value_;
  }

  ALWAYS_INLINE CodeOffset() = default;
  ALWAYS_INLINE CodeOffset(const CodeOffset&) = default;
  ALWAYS_INLINE CodeOffset& operator=(const CodeOffset&) = default;
  ALWAYS_INLINE CodeOffset& operator=(CodeOffset&&) = default;

 private:
  ALWAYS_INLINE explicit CodeOffset(uint32_t value) : value_(value) {}

  uint32_t value_ = 0u;
};

inline bool operator==(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() == b.CompressedValue();
}

inline bool operator!=(const CodeOffset& a, const CodeOffset& b) {
  return !(a == b);
}

inline bool operator<(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() < b.CompressedValue();
}

inline bool operator<=(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() <= b.CompressedValue();
}

inline bool operator>(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() > b.CompressedValue();
}

inline bool operator>=(const CodeOffset& a, const CodeOffset& b) {
  return a.CompressedValue() >= b.CompressedValue();
}

inline std::ostream& operator<<(std::ostream& os, const CodeOffset& offset) {
  return os << offset.Uint32Value();
}

}  // namespace art

#endif  // ART_RUNTIME_ARCH_CODE_OFFSET_H_
