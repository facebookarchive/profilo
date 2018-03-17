/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_IFTABLE_H_
#define ART_RUNTIME_MIRROR_IFTABLE_H_

#include "base/casts.h"
#include "object_array.h"

namespace art {
namespace mirror {

class MANAGED IfTable FINAL : public ObjectArray<Object> {
 public:
  ALWAYS_INLINE Class* GetInterface(int32_t i) SHARED_REQUIRES(Locks::mutator_lock_) {
    Class* interface = GetWithoutChecks((i * kMax) + kInterface)->AsClass();
    DCHECK(interface != nullptr);
    return interface;
  }

  ALWAYS_INLINE void SetInterface(int32_t i, Class* interface)
      SHARED_REQUIRES(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  PointerArray* GetMethodArray(int32_t i) SHARED_REQUIRES(Locks::mutator_lock_) {
    auto* method_array = down_cast<PointerArray*>(Get<kVerifyFlags, kReadBarrierOption>(
        (i * kMax) + kMethodArray));
    DCHECK(method_array != nullptr);
    return method_array;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  size_t GetMethodArrayCount(int32_t i) SHARED_REQUIRES(Locks::mutator_lock_) {
    auto* method_array = down_cast<PointerArray*>(
        Get<kVerifyFlags, kReadBarrierOption>((i * kMax) + kMethodArray));
    return method_array == nullptr ? 0u : method_array->GetLength();
  }

  void SetMethodArray(int32_t i, PointerArray* arr) SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(arr != nullptr);
    auto idx = i * kMax + kMethodArray;
    DCHECK(Get(idx) == nullptr);
    Set<false>(idx, arr);
  }

  size_t Count() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetLength() / kMax;
  }

  enum {
    // Points to the interface class.
    kInterface   = 0,
    // Method pointers into the vtable, allow fast map from interface method index to concrete
    // instance method.
    kMethodArray = 1,
    kMax         = 2,
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IfTable);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_IFTABLE_H_
