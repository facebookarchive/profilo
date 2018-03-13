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

#ifndef ART_RUNTIME_GC_SCOPED_GC_CRITICAL_SECTION_H_
#define ART_RUNTIME_GC_SCOPED_GC_CRITICAL_SECTION_H_

#include "base/mutex.h"
#include "collector_type.h"
#include "gc_cause.h"

namespace art {

class Thread;

namespace gc {

// Wait until the GC is finished and then prevent GC from starting until the destructor. Used
// to prevent deadlocks in places where we call ClassLinker::VisitClass with all th threads
// suspended.
class ScopedGCCriticalSection {
 public:
  ScopedGCCriticalSection(Thread* self, GcCause cause, CollectorType collector_type)
      ACQUIRE(Roles::uninterruptible_);
  ~ScopedGCCriticalSection() RELEASE(Roles::uninterruptible_);

 private:
  Thread* const self_;
  const char* old_cause_;
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SCOPED_GC_CRITICAL_SECTION_H_
