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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ALLOC_ENTRYPOINTS_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ALLOC_ENTRYPOINTS_H_

#include "gc/heap.h"
#include "quick_entrypoints.h"

namespace art {

namespace gc {
enum AllocatorType;
}  // namespace gc

void ResetQuickAllocEntryPoints(QuickEntryPoints* qpoints);

// Runtime shutdown lock is necessary to prevent races in thread initialization. When the thread is
// starting it doesn't hold the mutator lock until after it has been added to the thread list.
// However, Thread::Init is guarded by the runtime shutdown lock, so we can prevent these races by
// holding the runtime shutdown lock and the mutator lock when we update the entrypoints.

void SetQuickAllocEntryPointsAllocator(gc::AllocatorType allocator)
    EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::runtime_shutdown_lock_);

void SetQuickAllocEntryPointsInstrumented(bool instrumented)
    EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_, Locks::runtime_shutdown_lock_);

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ALLOC_ENTRYPOINTS_H_
