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

#include <museum/8.1.0/external/libcxx/list>
#include <museum/8.1.0/external/libcxx/memory>

#include <museum/8.1.0/art/runtime/base/macros.h>
#include <museum/8.1.0/art/runtime/base/mutex.h>
#include <museum/8.1.0/art/runtime/obj_ptr.h>
#include <museum/8.1.0/art/runtime/gc_root.h>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

namespace mirror {
  class Object;
}  // namespace mirror

class Thread;

namespace gc {

class AllocationListener {
 public:
  virtual ~AllocationListener() {}

  virtual void ObjectAllocated(Thread* self, ObjPtr<mirror::Object>* obj, size_t byte_count)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
};

}  // namespace gc
} } } } // namespace facebook::museum::MUSEUM_VERSION::art

#endif  // ART_RUNTIME_GC_ALLOCATION_LISTENER_H_
