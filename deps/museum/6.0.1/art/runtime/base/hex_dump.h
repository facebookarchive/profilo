/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_HEX_DUMP_H_
#define ART_RUNTIME_BASE_HEX_DUMP_H_

#include "macros.h"

#include <ostream>

namespace art {

// Prints a hex dump in this format:
//
// 01234560: 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff  0123456789abcdef
// 01234568: 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff  0123456789abcdef
class HexDump {
 public:
  HexDump(const void* address, size_t byte_count, bool show_actual_addresses, const char* prefix)
      : address_(address), byte_count_(byte_count), show_actual_addresses_(show_actual_addresses),
        prefix_(prefix) {
  }

  void Dump(std::ostream& os) const;

 private:
  const void* const address_;
  const size_t byte_count_;
  const bool show_actual_addresses_;
  const char* const prefix_;

  DISALLOW_COPY_AND_ASSIGN(HexDump);
};

inline std::ostream& operator<<(std::ostream& os, const HexDump& rhs) {
  rhs.Dump(os);
  return os;
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_HEX_DUMP_H_
