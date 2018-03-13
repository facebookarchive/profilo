/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_OBJECT_CALLBACKS_H_
#define ART_RUNTIME_OBJECT_CALLBACKS_H_

#include "base/macros.h"

namespace art {
namespace mirror {
  class Object;
  template<class MirrorType> class HeapReference;
}  // namespace mirror

// A callback for visiting an object in the heap.
typedef void (ObjectCallback)(mirror::Object* obj, void* arg);

class IsMarkedVisitor {
 public:
  virtual ~IsMarkedVisitor() {}
  // Return null if an object is not marked, otherwise returns the new address of that object.
  // May return the same address as the input if the object did not move.
  virtual mirror::Object* IsMarked(mirror::Object* obj) = 0;
};

class MarkObjectVisitor {
 public:
  virtual ~MarkObjectVisitor() {}
  // Mark an object and return the new address of an object.
  // May return the same address as the input if the object did not move.
  virtual mirror::Object* MarkObject(mirror::Object* obj) = 0;
  // Mark an object and update the value stored in the heap reference if the object moved.
  virtual void MarkHeapReference(mirror::HeapReference<mirror::Object>* obj) = 0;
};

}  // namespace art

#endif  // ART_RUNTIME_OBJECT_CALLBACKS_H_
