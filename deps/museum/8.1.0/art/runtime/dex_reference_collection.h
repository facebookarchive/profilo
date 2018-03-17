/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_DEX_REFERENCE_COLLECTION_H_
#define ART_RUNTIME_DEX_REFERENCE_COLLECTION_H_

#include "base/macros.h"

#include <vector>
#include <map>

namespace art {

class DexFile;

// Collection of dex references that is more memory efficient than a vector of <dex, index> pairs.
// Also allows quick lookups of all of the references for a single dex.
template <class IndexType, template<typename Type> class Allocator>
class DexReferenceCollection {
 public:
  using VectorAllocator = Allocator<IndexType>;
  using IndexVector = std::vector<IndexType, VectorAllocator>;
  using MapAllocator = Allocator<std::pair<const DexFile*, IndexVector>>;
  using DexFileMap = std::map<
      const DexFile*,
      IndexVector,
      std::less<const DexFile*>,
      Allocator<std::pair<const DexFile* const, IndexVector>>>;

  DexReferenceCollection(const MapAllocator& map_allocator = MapAllocator(),
                         const VectorAllocator& vector_allocator = VectorAllocator())
      : map_(map_allocator),
        vector_allocator_(vector_allocator) {}

  void AddReference(const DexFile* dex, IndexType index) {
    GetOrInsertVector(dex)->push_back(index);
  }

  DexFileMap& GetMap() {
    return map_;
  }

  size_t NumReferences() const {
    size_t ret = 0;
    for (auto&& pair : map_) {
      ret += pair.second.size();
    }
    return ret;
  }

 private:
  DexFileMap map_;
  const DexFile* current_dex_file_ = nullptr;
  IndexVector* current_vector_ = nullptr;
  VectorAllocator vector_allocator_;

  ALWAYS_INLINE IndexVector* GetOrInsertVector(const DexFile* dex) {
    // Optimize for adding to same vector in succession, the cached dex file and vector aims to
    // prevent map lookups.
    if (UNLIKELY(current_dex_file_ != dex)) {
      // There is an assumption that constructing an empty vector wont do any allocations. If this
      // incorrect, this might leak for the arena case.
      current_vector_ = &map_.emplace(dex, IndexVector(vector_allocator_)).first->second;
      current_dex_file_ = dex;
    }
    return current_vector_;
  }
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_REFERENCE_COLLECTION_H_
