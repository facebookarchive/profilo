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

#include "base/mutex.h"
#include "handle.h"
#include "obj_ptr-inl.h"
#include "thread-current-inl.h"
#include "verify_object.h"

namespace art {

template<size_t kNumReferences>
inline FixedSizeHandleScope<kNumReferences>::FixedSizeHandleScope(BaseHandleScope* link,
                                                                  mirror::Object* fill_value)
    : HandleScope(link, kNumReferences) {
  if (kDebugLocking) {
    Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  }
  static_assert(kNumReferences >= 1, "FixedSizeHandleScope must contain at least 1 reference");
  DCHECK_EQ(&storage_[0], GetReferences());  // TODO: Figure out how to use a compile assert.
  for (size_t i = 0; i < kNumReferences; ++i) {
    SetReference(i, fill_value);
  }
}

template<size_t kNumReferences>
inline StackHandleScope<kNumReferences>::StackHandleScope(Thread* self, mirror::Object* fill_value)
    : FixedSizeHandleScope<kNumReferences>(self->GetTopHandleScope(), fill_value),
      self_(self) {
  DCHECK_EQ(self, Thread::Current());
  self_->PushHandleScope(this);
}

template<size_t kNumReferences>
inline StackHandleScope<kNumReferences>::~StackHandleScope() {
  BaseHandleScope* top_handle_scope = self_->PopHandleScope();
  DCHECK_EQ(top_handle_scope, this);
  if (kDebugLocking) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
  }
}

inline size_t HandleScope::SizeOf(uint32_t num_references) {
  size_t header_size = sizeof(HandleScope);
  size_t data_size = sizeof(StackReference<mirror::Object>) * num_references;
  return header_size + data_size;
}

inline size_t HandleScope::SizeOf(PointerSize pointer_size, uint32_t num_references) {
  // Assume that the layout is packed.
  size_t header_size = ReferencesOffset(pointer_size);
  size_t data_size = sizeof(StackReference<mirror::Object>) * num_references;
  return header_size + data_size;
}

inline mirror::Object* HandleScope::GetReference(size_t i) const {
  DCHECK_LT(i, NumberOfReferences());
  if (kDebugLocking) {
    Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  }
  return GetReferences()[i].AsMirrorPtr();
}

inline Handle<mirror::Object> HandleScope::GetHandle(size_t i) {
  DCHECK_LT(i, NumberOfReferences());
  return Handle<mirror::Object>(&GetReferences()[i]);
}

inline MutableHandle<mirror::Object> HandleScope::GetMutableHandle(size_t i) {
  DCHECK_LT(i, NumberOfReferences());
  return MutableHandle<mirror::Object>(&GetReferences()[i]);
}

inline void HandleScope::SetReference(size_t i, mirror::Object* object) {
  if (kDebugLocking) {
    Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  }
  DCHECK_LT(i, NumberOfReferences());
  GetReferences()[i].Assign(object);
}

inline bool HandleScope::Contains(StackReference<mirror::Object>* handle_scope_entry) const {
  // A HandleScope should always contain something. One created by the
  // jni_compiler should have a jobject/jclass as a native method is
  // passed in a this pointer or a class
  DCHECK_GT(NumberOfReferences(), 0U);
  return &GetReferences()[0] <= handle_scope_entry &&
      handle_scope_entry <= &GetReferences()[number_of_references_ - 1];
}

template<size_t kNumReferences> template<class T>
inline MutableHandle<T> FixedSizeHandleScope<kNumReferences>::NewHandle(T* object) {
  SetReference(pos_, object);
  MutableHandle<T> h(GetHandle<T>(pos_));
  pos_++;
  return h;
}

template<size_t kNumReferences> template<class MirrorType>
inline MutableHandle<MirrorType> FixedSizeHandleScope<kNumReferences>::NewHandle(
    ObjPtr<MirrorType> object) {
  return NewHandle(object.Ptr());
}

template<size_t kNumReferences> template<class T>
inline HandleWrapper<T> FixedSizeHandleScope<kNumReferences>::NewHandleWrapper(T** object) {
  return HandleWrapper<T>(object, NewHandle(*object));
}

template<size_t kNumReferences> template<class T>
inline HandleWrapperObjPtr<T> FixedSizeHandleScope<kNumReferences>::NewHandleWrapper(
    ObjPtr<T>* object) {
  return HandleWrapperObjPtr<T>(object, NewHandle(*object));
}

template<size_t kNumReferences>
inline void FixedSizeHandleScope<kNumReferences>::SetReference(size_t i, mirror::Object* object) {
  if (kDebugLocking) {
    Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  }
  DCHECK_LT(i, kNumReferences);
  VerifyObject(object);
  GetReferences()[i].Assign(object);
}

// Number of references contained within this handle scope.
inline uint32_t BaseHandleScope::NumberOfReferences() const {
  return LIKELY(!IsVariableSized())
      ? AsHandleScope()->NumberOfReferences()
      : AsVariableSized()->NumberOfReferences();
}

inline bool BaseHandleScope::Contains(StackReference<mirror::Object>* handle_scope_entry) const {
  return LIKELY(!IsVariableSized())
      ? AsHandleScope()->Contains(handle_scope_entry)
      : AsVariableSized()->Contains(handle_scope_entry);
}

template <typename Visitor>
inline void BaseHandleScope::VisitRoots(Visitor& visitor) {
  if (LIKELY(!IsVariableSized())) {
    AsHandleScope()->VisitRoots(visitor);
  } else {
    AsVariableSized()->VisitRoots(visitor);
  }
}

inline VariableSizedHandleScope* BaseHandleScope::AsVariableSized() {
  DCHECK(IsVariableSized());
  return down_cast<VariableSizedHandleScope*>(this);
}

inline HandleScope* BaseHandleScope::AsHandleScope() {
  DCHECK(!IsVariableSized());
  return down_cast<HandleScope*>(this);
}

inline const VariableSizedHandleScope* BaseHandleScope::AsVariableSized() const {
  DCHECK(IsVariableSized());
  return down_cast<const VariableSizedHandleScope*>(this);
}

inline const HandleScope* BaseHandleScope::AsHandleScope() const {
  DCHECK(!IsVariableSized());
  return down_cast<const HandleScope*>(this);
}

template<class T>
MutableHandle<T> VariableSizedHandleScope::NewHandle(T* object) {
  if (current_scope_->RemainingSlots() == 0) {
    current_scope_ = new LocalScopeType(current_scope_);
  }
  return current_scope_->NewHandle(object);
}

template<class MirrorType>
inline MutableHandle<MirrorType> VariableSizedHandleScope::NewHandle(ObjPtr<MirrorType> ptr) {
  return NewHandle(ptr.Ptr());
}

inline VariableSizedHandleScope::VariableSizedHandleScope(Thread* const self)
    : BaseHandleScope(self->GetTopHandleScope()),
      self_(self) {
  current_scope_ = new LocalScopeType(/*link*/ nullptr);
  self_->PushHandleScope(this);
}

inline VariableSizedHandleScope::~VariableSizedHandleScope() {
  BaseHandleScope* top_handle_scope = self_->PopHandleScope();
  DCHECK_EQ(top_handle_scope, this);
  while (current_scope_ != nullptr) {
    LocalScopeType* next = reinterpret_cast<LocalScopeType*>(current_scope_->GetLink());
    delete current_scope_;
    current_scope_ = next;
  }
}

inline uint32_t VariableSizedHandleScope::NumberOfReferences() const {
  uint32_t sum = 0;
  const LocalScopeType* cur = current_scope_;
  while (cur != nullptr) {
    sum += cur->NumberOfReferences();
    cur = reinterpret_cast<const LocalScopeType*>(cur->GetLink());
  }
  return sum;
}

inline bool VariableSizedHandleScope::Contains(StackReference<mirror::Object>* handle_scope_entry)
    const {
  const LocalScopeType* cur = current_scope_;
  while (cur != nullptr) {
    if (cur->Contains(handle_scope_entry)) {
      return true;
    }
    cur = reinterpret_cast<const LocalScopeType*>(cur->GetLink());
  }
  return false;
}

template <typename Visitor>
inline void VariableSizedHandleScope::VisitRoots(Visitor& visitor) {
  LocalScopeType* cur = current_scope_;
  while (cur != nullptr) {
    cur->VisitRoots(visitor);
    cur = reinterpret_cast<LocalScopeType*>(cur->GetLink());
  }
}


}  // namespace art

#endif  // ART_RUNTIME_HANDLE_SCOPE_INL_H_
