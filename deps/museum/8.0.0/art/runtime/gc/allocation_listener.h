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

#ifndef ART_RUNTIME_GC_ALLOCATION_LISTENER_H_
#define ART_RUNTIME_GC_ALLOCATION_LISTENER_H_

#include <list>
#include <memory>

#include "base/macros.h"
#include "base/mutex.h"
#include "obj_ptr.h"
#include "object_callbacks.h"
#include "gc_root.h"

namespace art {

namespace mirror {
  class Object;
}

class Thread;

namespace gc {

class AllocationListener {
 public:
  virtual ~AllocationListener() {}

  virtual void ObjectAllocated(Thread* self, ObjPtr<mirror::Object>* obj, size_t byte_count)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ALLOCATION_LISTENER_H_
