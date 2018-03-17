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

#ifndef ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
#define ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_

#include <inttypes.h>
#include <memory>
#include <vector>

namespace art {

extern "C" {
  struct JITCodeEntry;
}

// Notify native debugger about new JITed code by passing in-memory ELF.
// It takes ownership of the in-memory ELF file.
JITCodeEntry* CreateJITCodeEntry(std::vector<uint8_t> symfile);

// Notify native debugger that JITed code has been removed.
// It also releases the associated in-memory ELF file.
void DeleteJITCodeEntry(JITCodeEntry* entry);

// Notify native debugger about new JITed code by passing in-memory ELF.
// The address is used only to uniquely identify the entry.
// It takes ownership of the in-memory ELF file.
void CreateJITCodeEntryForAddress(uintptr_t address, std::vector<uint8_t> symfile);

// Notify native debugger that JITed code has been removed.
// Returns false if entry for the given address was not found.
bool DeleteJITCodeEntryForAddress(uintptr_t address);

}  // namespace art

#endif  // ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
