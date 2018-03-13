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

#ifndef ART_RUNTIME_BASE_ARENA_OBJECT_H_
#define ART_RUNTIME_BASE_ARENA_OBJECT_H_

#include "base/arena_allocator.h"
#include "base/logging.h"
#include "scoped_arena_allocator.h"

namespace art {

// Parent for arena allocated objects giving appropriate new and delete operators.
template<enum ArenaAllocKind kAllocKind>
class ArenaObject {
 public:
  // Allocate a new ArenaObject of 'size' bytes in the Arena.
  void* operator new(size_t size, ArenaAllocator* allocator) {
    return allocator->Alloc(size, kAllocKind);
  }

  static void* operator new(size_t size, ScopedArenaAllocator* arena) {
    return arena->Alloc(size, kAllocKind);
  }

  void operator delete(void*, size_t) {
    LOG(FATAL) << "UNREACHABLE";
    UNREACHABLE();
  }
};


// Parent for arena allocated objects that get deleted, gives appropriate new and delete operators.
// Currently this is used by the quick compiler for debug reference counting arena allocations.
template<enum ArenaAllocKind kAllocKind>
class DeletableArenaObject {
 public:
  // Allocate a new ArenaObject of 'size' bytes in the Arena.
  void* operator new(size_t size, ArenaAllocator* allocator) {
    return allocator->Alloc(size, kAllocKind);
  }

  static void* operator new(size_t size, ScopedArenaAllocator* arena) {
    return arena->Alloc(size, kAllocKind);
  }

  void operator delete(void*, size_t) {
    // Nop.
  }
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_ARENA_OBJECT_H_
