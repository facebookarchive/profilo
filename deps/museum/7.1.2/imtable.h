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

#ifndef ART_RUNTIME_IMTABLE_H_
#define ART_RUNTIME_IMTABLE_H_

#ifndef IMT_SIZE
#error IMT_SIZE not defined
#endif

namespace art {

class ArtMethod;

class ImTable {
 public:
  // Interface method table size. Increasing this value reduces the chance of two interface methods
  // colliding in the interface method table but increases the size of classes that implement
  // (non-marker) interfaces.
  static constexpr size_t kSize = IMT_SIZE;

  ArtMethod* Get(size_t index, size_t pointer_size) {
    DCHECK_LT(index, kSize);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(this) + OffsetOfElement(index, pointer_size);
    if (pointer_size == 4) {
      uint32_t value = *reinterpret_cast<uint32_t*>(ptr);
      return reinterpret_cast<ArtMethod*>(value);
    } else {
      uint64_t value = *reinterpret_cast<uint64_t*>(ptr);
      return reinterpret_cast<ArtMethod*>(value);
    }
  }

  void Set(size_t index, ArtMethod* method, size_t pointer_size) {
    DCHECK_LT(index, kSize);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(this) + OffsetOfElement(index, pointer_size);
    if (pointer_size == 4) {
      uintptr_t value = reinterpret_cast<uintptr_t>(method);
      DCHECK_EQ(static_cast<uint32_t>(value), value);  // Check that we dont lose any non 0 bits.
      *reinterpret_cast<uint32_t*>(ptr) = static_cast<uint32_t>(value);
    } else {
      *reinterpret_cast<uint64_t*>(ptr) = reinterpret_cast<uint64_t>(method);
    }
  }

  static size_t OffsetOfElement(size_t index, size_t pointer_size) {
    return index * pointer_size;
  }

  void Populate(ArtMethod** data, size_t pointer_size) {
    for (size_t i = 0; i < kSize; ++i) {
      Set(i, data[i], pointer_size);
    }
  }

  constexpr static size_t SizeInBytes(size_t pointer_size) {
    return kSize * pointer_size;
  }
};

}  // namespace art

#endif  // ART_RUNTIME_IMTABLE_H_

