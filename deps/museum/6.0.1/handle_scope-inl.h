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

#ifndef ART_RUNTIME_HANDLE_SCOPE_INL_H_
#define ART_RUNTIME_HANDLE_SCOPE_INL_H_

#include "handle_scope.h"

#include "handle.h"
#include "thread.h"
#include "verify_object-inl.h"

namespace art {

template<size_t kNumReferences>
inline StackHandleScope<kNumReferences>::StackHandleScope(Thread* self, mirror::Object* fill_value)
    : HandleScope(self->GetTopHandleScope(), kNumReferences), self_(self), pos_(0) {
  DCHECK_EQ(self, Thread::Current());
  static_assert(kNumReferences >= 1, "StackHandleScope must contain at least 1 reference");
  // TODO: Figure out how to use a compile assert.
  CHECK_EQ(&storage_[0], GetReferences());
  for (size_t i = 0; i < kNumReferences; ++i) {
    SetReference(i, fill_value);
  }
  self_->PushHandleScope(this);
}

template<size_t kNumReferences>
inline StackHandleScope<kNumReferences>::~StackHandleScope() {
  HandleScope* top_handle_scope = self_->PopHandleScope();
  DCHECK_EQ(top_handle_scope, this);
}

inline size_t HandleScope::SizeOf(uint32_t num_references) {
  size_t header_size = sizeof(HandleScope);
  size_t data_size = sizeof(StackReference<mirror::Object>) * num_references;
  return header_size + data_size;
}

inline size_t HandleScope::SizeOf(size_t pointer_size, uint32_t num_references) {
  // Assume that the layout is packed.
  size_t header_size = pointer_size + sizeof(number_of_references_);
  size_t data_size = sizeof(StackReference<mirror::Object>) * num_references;
  return header_size + data_size;
}

inline mirror::Object* HandleScope::GetReference(size_t i) const {
  DCHECK_LT(i, number_of_references_);
  return GetReferences()[i].AsMirrorPtr();
}

inline Handle<mirror::Object> HandleScope::GetHandle(size_t i) {
  DCHECK_LT(i, number_of_references_);
  return Handle<mirror::Object>(&GetReferences()[i]);
}

inline MutableHandle<mirror::Object> HandleScope::GetMutableHandle(size_t i) {
  DCHECK_LT(i, number_of_references_);
  return MutableHandle<mirror::Object>(&GetReferences()[i]);
}

inline void HandleScope::SetReference(size_t i, mirror::Object* object) {
  DCHECK_LT(i, number_of_references_);
  GetReferences()[i].Assign(object);
}

inline bool HandleScope::Contains(StackReference<mirror::Object>* handle_scope_entry) const {
  // A HandleScope should always contain something. One created by the
  // jni_compiler should have a jobject/jclass as a native method is
  // passed in a this pointer or a class
  DCHECK_GT(number_of_references_, 0U);
  return &GetReferences()[0] <= handle_scope_entry &&
      handle_scope_entry <= &GetReferences()[number_of_references_ - 1];
}

template<size_t kNumReferences> template<class T>
inline MutableHandle<T> StackHandleScope<kNumReferences>::NewHandle(T* object) {
  SetReference(pos_, object);
  MutableHandle<T> h(GetHandle<T>(pos_));
  pos_++;
  return h;
}

template<size_t kNumReferences> template<class T>
inline HandleWrapper<T> StackHandleScope<kNumReferences>::NewHandleWrapper(T** object) {
  SetReference(pos_, *object);
  MutableHandle<T> h(GetHandle<T>(pos_));
  pos_++;
  return HandleWrapper<T>(object, h);
}

template<size_t kNumReferences>
inline void StackHandleScope<kNumReferences>::SetReference(size_t i, mirror::Object* object) {
  DCHECK_LT(i, kNumReferences);
  VerifyObject(object);
  GetReferences()[i].Assign(object);
}

}  // namespace art

#endif  // ART_RUNTIME_HANDLE_SCOPE_INL_H_
