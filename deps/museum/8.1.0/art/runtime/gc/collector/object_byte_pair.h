/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_OBJECT_BYTE_PAIR_H_
#define ART_RUNTIME_GC_COLLECTOR_OBJECT_BYTE_PAIR_H_

#include <inttypes.h>

namespace art {
namespace gc {
namespace collector {

struct ObjectBytePair {
  explicit ObjectBytePair(uint64_t num_objects = 0, int64_t num_bytes = 0)
      : objects(num_objects), bytes(num_bytes) {}
  void Add(const ObjectBytePair& other) {
    objects += other.objects;
    bytes += other.bytes;
  }
  // Number of objects which were freed.
  uint64_t objects;
  // Freed bytes are signed since the GC can free negative bytes if it promotes objects to a space
  // which has a larger allocation size.
  int64_t bytes;
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_OBJECT_BYTE_PAIR_H_
