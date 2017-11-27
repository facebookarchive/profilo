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

#ifndef ART_RUNTIME_HANDLE_SCOPE_H_
#define ART_RUNTIME_HANDLE_SCOPE_H_

#include <stack>

#include "base/logging.h"
#include "base/macros.h"
#include "handle.h"
#include "stack.h"
#include "verify_object.h"

namespace art {
namespace mirror {
class Object;
}

class Thread;

// HandleScopes are scoped objects containing a number of Handles. They are used to allocate
// handles, for these handles (and the objects contained within them) to be visible/roots for the
// GC. It is most common to stack allocate HandleScopes using StackHandleScope.
class PACKED(4) HandleScope {
 public:
  ~HandleScope() {}

  // Number of references contained within this handle scope.
  uint32_t NumberOfReferences() const {
    return number_of_references_;
  }

  // We have versions with and without explicit pointer size of the following. The first two are
  // used at runtime, so OFFSETOF_MEMBER computes the right offsets automatically. The last one
  // takes the pointer size explicitly so that at compile time we can cross-compile correctly.

  // Returns the size of a HandleScope containing num_references handles.
  static size_t SizeOf(uint32_t num_references);

  // Returns the size of a HandleScope containing num_references handles.
  static size_t SizeOf(size_t pointer_size, uint32_t num_references);

  // Link to previous HandleScope or null.
  HandleScope* GetLink() const {
    return link_;
  }

  ALWAYS_INLINE mirror::Object* GetReference(size_t i) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE Handle<mirror::Object> GetHandle(size_t i);

  ALWAYS_INLINE MutableHandle<mirror::Object> GetMutableHandle(size_t i)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE void SetReference(size_t i, mirror::Object* object)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE bool Contains(StackReference<mirror::Object>* handle_scope_entry) const;

  // Offset of link within HandleScope, used by generated code.
  static size_t LinkOffset(size_t pointer_size ATTRIBUTE_UNUSED) {
    return 0;
  }

  // Offset of length within handle scope, used by generated code.
  static size_t NumberOfReferencesOffset(size_t pointer_size) {
    return pointer_size;
  }

  // Offset of link within handle scope, used by generated code.
  static size_t ReferencesOffset(size_t pointer_size) {
    return pointer_size + sizeof(number_of_references_);
  }

  // Placement new creation.
  static HandleScope* Create(void* storage, HandleScope* link, uint32_t num_references)
      WARN_UNUSED {
    return new (storage) HandleScope(link, num_references);
  }

 protected:
  // Return backing storage used for references.
  ALWAYS_INLINE StackReference<mirror::Object>* GetReferences() const {
    uintptr_t address = reinterpret_cast<uintptr_t>(this) + ReferencesOffset(sizeof(void*));
    return reinterpret_cast<StackReference<mirror::Object>*>(address);
  }

  explicit HandleScope(size_t number_of_references) :
      link_(nullptr), number_of_references_(number_of_references) {
  }

  // Semi-hidden constructor. Construction expected by generated code and StackHandleScope.
  HandleScope(HandleScope* link, uint32_t num_references) :
      link_(link), number_of_references_(num_references) {
  }

  // Link-list of handle scopes. The root is held by a Thread.
  HandleScope* const link_;

  // Number of handlerized references.
  const uint32_t number_of_references_;

  // Storage for references.
  // StackReference<mirror::Object> references_[number_of_references_]

 private:
  DISALLOW_COPY_AND_ASSIGN(HandleScope);
};

// A wrapper which wraps around Object** and restores the pointer in the destructor.
// TODO: Add more functionality.
template<class T>
class HandleWrapper : public MutableHandle<T> {
 public:
  HandleWrapper(T** obj, const MutableHandle<T>& handle)
     : MutableHandle<T>(handle), obj_(obj) {
  }

  HandleWrapper(const HandleWrapper&) = default;

  ~HandleWrapper() {
    *obj_ = MutableHandle<T>::Get();
  }

 private:
  T** const obj_;
};

// Scoped handle storage of a fixed size that is usually stack allocated.
template<size_t kNumReferences>
class PACKED(4) StackHandleScope FINAL : public HandleScope {
 public:
  explicit ALWAYS_INLINE StackHandleScope(Thread* self, mirror::Object* fill_value = nullptr);
  ALWAYS_INLINE ~StackHandleScope();

  template<class T>
  ALWAYS_INLINE MutableHandle<T> NewHandle(T* object) SHARED_REQUIRES(Locks::mutator_lock_);

  template<class T>
  ALWAYS_INLINE HandleWrapper<T> NewHandleWrapper(T** object)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ALWAYS_INLINE void SetReference(size_t i, mirror::Object* object)
      SHARED_REQUIRES(Locks::mutator_lock_);

  Thread* Self() const {
    return self_;
  }

 private:
  template<class T>
  ALWAYS_INLINE MutableHandle<T> GetHandle(size_t i) SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK_LT(i, kNumReferences);
    return MutableHandle<T>(&GetReferences()[i]);
  }

  // Reference storage needs to be first as expected by the HandleScope layout.
  StackReference<mirror::Object> storage_[kNumReferences];

  // The thread that the stack handle scope is a linked list upon. The stack handle scope will
  // push and pop itself from this thread.
  Thread* const self_;

  // Position new handles will be created.
  size_t pos_;

  template<size_t kNumRefs> friend class StackHandleScope;
};

// Utility class to manage a collection (stack) of StackHandleScope. All the managed
// scope handle have the same fixed sized.
// Calls to NewHandle will create a new handle inside the top StackHandleScope.
// When the handle scope becomes full a new one is created and push on top of the
// previous.
//
// NB:
// - it is not safe to use the *same* StackHandleScopeCollection intermix with
// other StackHandleScopes.
// - this is a an easy way around implementing a full ZoneHandleScope to manage an
// arbitrary number of handles.
class StackHandleScopeCollection {
 public:
  explicit StackHandleScopeCollection(Thread* const self) :
      self_(self),
      current_scope_num_refs_(0) {
  }

  ~StackHandleScopeCollection() {
    while (!scopes_.empty()) {
      delete scopes_.top();
      scopes_.pop();
    }
  }

  template<class T>
  MutableHandle<T> NewHandle(T* object) SHARED_REQUIRES(Locks::mutator_lock_) {
    if (scopes_.empty() || current_scope_num_refs_ >= kNumReferencesPerScope) {
      StackHandleScope<kNumReferencesPerScope>* scope =
          new StackHandleScope<kNumReferencesPerScope>(self_);
      scopes_.push(scope);
      current_scope_num_refs_ = 0;
    }
    current_scope_num_refs_++;
    return scopes_.top()->NewHandle(object);
  }

 private:
  static constexpr size_t kNumReferencesPerScope = 4;

  Thread* const self_;

  std::stack<StackHandleScope<kNumReferencesPerScope>*> scopes_;
  size_t current_scope_num_refs_;

  DISALLOW_COPY_AND_ASSIGN(StackHandleScopeCollection);
};

}  // namespace art

#endif  // ART_RUNTIME_HANDLE_SCOPE_H_
