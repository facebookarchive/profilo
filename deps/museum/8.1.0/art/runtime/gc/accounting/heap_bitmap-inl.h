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

#ifndef ART_RUNTIME_GC_ACCOUNTING_HEAP_BITMAP_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_HEAP_BITMAP_INL_H_

#include "heap_bitmap.h"

#include "space_bitmap-inl.h"

namespace art {
namespace gc {
namespace accounting {

template <typename Visitor>
inline void HeapBitmap::Visit(Visitor&& visitor) {
  for (const auto& bitmap : continuous_space_bitmaps_) {
    bitmap->VisitMarkedRange(bitmap->HeapBegin(), bitmap->HeapLimit(), visitor);
  }
  for (const auto& bitmap : large_object_bitmaps_) {
    bitmap->VisitMarkedRange(bitmap->HeapBegin(), bitmap->HeapLimit(), visitor);
  }
}

inline bool HeapBitmap::Test(const mirror::Object* obj) {
  ContinuousSpaceBitmap* bitmap = GetContinuousSpaceBitmap(obj);
  if (LIKELY(bitmap != nullptr)) {
    return bitmap->Test(obj);
  }
  for (const auto& lo_bitmap : large_object_bitmaps_) {
    if (LIKELY(lo_bitmap->HasAddress(obj))) {
      return lo_bitmap->Test(obj);
    }
  }
  LOG(FATAL) << "Invalid object " << obj;
  return false;
}

inline void HeapBitmap::Clear(const mirror::Object* obj) {
  ContinuousSpaceBitmap* bitmap = GetContinuousSpaceBitmap(obj);
  if (LIKELY(bitmap != nullptr)) {
    bitmap->Clear(obj);
    return;
  }
  for (const auto& lo_bitmap : large_object_bitmaps_) {
    if (LIKELY(lo_bitmap->HasAddress(obj))) {
      lo_bitmap->Clear(obj);
    }
  }
  LOG(FATAL) << "Invalid object " << obj;
}

template<typename LargeObjectSetVisitor>
inline bool HeapBitmap::Set(const mirror::Object* obj, const LargeObjectSetVisitor& visitor) {
  ContinuousSpaceBitmap* bitmap = GetContinuousSpaceBitmap(obj);
  if (LIKELY(bitmap != nullptr)) {
    return bitmap->Set(obj);
  }
  visitor(obj);
  for (const auto& lo_bitmap : large_object_bitmaps_) {
    if (LIKELY(lo_bitmap->HasAddress(obj))) {
      return lo_bitmap->Set(obj);
    }
  }
  LOG(FATAL) << "Invalid object " << obj;
  return false;
}

template<typename LargeObjectSetVisitor>
inline bool HeapBitmap::AtomicTestAndSet(const mirror::Object* obj,
                                         const LargeObjectSetVisitor& visitor) {
  ContinuousSpaceBitmap* bitmap = GetContinuousSpaceBitmap(obj);
  if (LIKELY(bitmap != nullptr)) {
    return bitmap->AtomicTestAndSet(obj);
  }
  visitor(obj);
  for (const auto& lo_bitmap : large_object_bitmaps_) {
    if (LIKELY(lo_bitmap->HasAddress(obj))) {
      return lo_bitmap->AtomicTestAndSet(obj);
    }
  }
  LOG(FATAL) << "Invalid object " << obj;
  return false;
}

inline ContinuousSpaceBitmap* HeapBitmap::GetContinuousSpaceBitmap(const mirror::Object* obj) const {
  for (const auto& bitmap : continuous_space_bitmaps_) {
    if (bitmap->HasAddress(obj)) {
      return bitmap;
    }
  }
  return nullptr;
}

inline LargeObjectBitmap* HeapBitmap::GetLargeObjectBitmap(const mirror::Object* obj) const {
  for (const auto& bitmap : large_object_bitmaps_) {
    if (LIKELY(bitmap->HasAddress(obj))) {
      return bitmap;
    }
  }
  return nullptr;
}

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_HEAP_BITMAP_INL_H_
