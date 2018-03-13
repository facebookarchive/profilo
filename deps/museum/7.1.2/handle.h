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

#ifndef ART_RUNTIME_HANDLE_H_
#define ART_RUNTIME_HANDLE_H_

#include "base/casts.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/value_object.h"
#include "stack.h"

namespace art {

class Thread;

template<class T> class Handle;

// Handles are memory locations that contain GC roots. As the mirror::Object*s within a handle are
// GC visible then the GC may move the references within them, something that couldn't be done with
// a wrap pointer. Handles are generally allocated within HandleScopes. Handle is a super-class
// of MutableHandle and doesn't support assignment operations.
template<class T>
class Handle : public ValueObject {
 public:
  Handle() : reference_(nullptr) {
  }

  ALWAYS_INLINE Handle(const Handle<T>& handle) : reference_(handle.reference_) {
  }

  ALWAYS_INLINE Handle<T>& operator=(const Handle<T>& handle) {
    reference_ = handle.reference_;
    return *this;
  }

  ALWAYS_INLINE explicit Handle(StackReference<T>* reference) : reference_(reference) {
  }

  ALWAYS_INLINE T& operator*() const SHARED_REQUIRES(Locks::mutator_lock_) {
    return *Get();
  }

  ALWAYS_INLINE T* operator->() const SHARED_REQUIRES(Locks::mutator_lock_) {
    return Get();
  }

  ALWAYS_INLINE T* Get() const SHARED_REQUIRES(Locks::mutator_lock_) {
    return down_cast<T*>(reference_->AsMirrorPtr());
  }

  ALWAYS_INLINE jobject ToJObject() const SHARED_REQUIRES(Locks::mutator_lock_) {
    if (UNLIKELY(reference_->AsMirrorPtr() == nullptr)) {
      // Special case so that we work with null handles.
      return nullptr;
    }
    return reinterpret_cast<jobject>(reference_);
  }

  ALWAYS_INLINE StackReference<mirror::Object>* GetReference() {
    return reference_;
  }

  ALWAYS_INLINE const StackReference<mirror::Object>* GetReference() const {
    return reference_;
  }

 protected:
  template<typename S>
  explicit Handle(StackReference<S>* reference)
      : reference_(reference) {
  }
  template<typename S>
  explicit Handle(const Handle<S>& handle)
      : reference_(handle.reference_) {
  }

  StackReference<mirror::Object>* reference_;

 private:
  friend class BuildGenericJniFrameVisitor;
  template<class S> friend class Handle;
  friend class HandleScope;
  template<class S> friend class HandleWrapper;
  template<size_t kNumReferences> friend class StackHandleScope;
};

// Handles that support assignment.
template<class T>
class MutableHandle : public Handle<T> {
 public:
  MutableHandle() {
  }

  ALWAYS_INLINE MutableHandle(const MutableHandle<T>& handle)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : Handle<T>(handle.reference_) {
  }

  ALWAYS_INLINE MutableHandle<T>& operator=(const MutableHandle<T>& handle)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    Handle<T>::operator=(handle);
    return *this;
  }

  ALWAYS_INLINE explicit MutableHandle(StackReference<T>* reference)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : Handle<T>(reference) {
  }

  ALWAYS_INLINE T* Assign(T* reference) SHARED_REQUIRES(Locks::mutator_lock_) {
    StackReference<mirror::Object>* ref = Handle<T>::GetReference();
    T* old = down_cast<T*>(ref->AsMirrorPtr());
    ref->Assign(reference);
    return old;
  }

  template<typename S>
  explicit MutableHandle(const MutableHandle<S>& handle) SHARED_REQUIRES(Locks::mutator_lock_)
      : Handle<T>(handle) {
  }

  template<typename S>
  explicit MutableHandle(StackReference<S>* reference) SHARED_REQUIRES(Locks::mutator_lock_)
      : Handle<T>(reference) {
  }

 private:
  friend class BuildGenericJniFrameVisitor;
  friend class HandleScope;
  template<class S> friend class HandleWrapper;
  template<size_t kNumReferences> friend class StackHandleScope;
};

// A special case of Handle that only holds references to null. Invalid when if it goes out of
// scope. Example: Handle<T> h = ScopedNullHandle<T> will leave h being undefined.
template<class T>
class ScopedNullHandle : public Handle<T> {
 public:
  ScopedNullHandle() : Handle<T>(&null_ref_) {}

 private:
  StackReference<mirror::Object> null_ref_;
};

}  // namespace art

#endif  // ART_RUNTIME_HANDLE_H_
