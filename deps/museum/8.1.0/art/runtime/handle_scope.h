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

#include "base/enums.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "handle.h"
#include "stack_reference.h"
#include "verify_object.h"

namespace art {

class HandleScope;
template<class MirrorType> class ObjPtr;
class Thread;
class VariableSizedHandleScope;

namespace mirror {
class Object;
}  // namespace mirror

// Basic handle scope, tracked by a list. May be variable sized.
class PACKED(4) BaseHandleScope {
 public:
  bool IsVariableSized() const {
    return number_of_references_ == kNumReferencesVariableSized;
  }

  // Number of references contained within this handle scope.
  ALWAYS_INLINE uint32_t NumberOfReferences() const;

  ALWAYS_INLINE bool Contains(StackReference<mirror::Object>* handle_scope_entry) const;

  template <typename Visitor>
  ALWAYS_INLINE void VisitRoots(Visitor& visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Link to previous BaseHandleScope or null.
  BaseHandleScope* GetLink() const {
    return link_;
  }

  ALWAYS_INLINE VariableSizedHandleScope* AsVariableSized();
  ALWAYS_INLINE HandleScope* AsHandleScope();
  ALWAYS_INLINE const VariableSizedHandleScope* AsVariableSized() const;
  ALWAYS_INLINE const HandleScope* AsHandleScope() const;

 protected:
  BaseHandleScope(BaseHandleScope* link, uint32_t num_references)
      : link_(link),
        number_of_references_(num_references) {}

  // Variable sized constructor.
  explicit BaseHandleScope(BaseHandleScope* link)
      : link_(link),
        number_of_references_(kNumReferencesVariableSized) {}

  static constexpr int32_t kNumReferencesVariableSized = -1;

  // Link-list of handle scopes. The root is held by a Thread.
  BaseHandleScope* const link_;

  // Number of handlerized references. -1 for variable sized handle scopes.
  const int32_t number_of_references_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseHandleScope);
};

// HandleScopes are scoped objects containing a number of Handles. They are used to allocate
// handles, for these handles (and the objects contained within them) to be visible/roots for the
// GC. It is most common to stack allocate HandleScopes using StackHandleScope.
class PACKED(4) HandleScope : public BaseHandleScope {
 public:
  ~HandleScope() {}

  // We have versions with and without explicit pointer size of the following. The first two are
  // used at runtime, so OFFSETOF_MEMBER computes the right offsets automatically. The last one
  // takes the pointer size explicitly so that at compile time we can cross-compile correctly.

  // Returns the size of a HandleScope containing num_references handles.
  static size_t SizeOf(uint32_t num_references);

  // Returns the size of a HandleScope containing num_references handles.
  static size_t SizeOf(PointerSize pointer_size, uint32_t num_references);

  ALWAYS_INLINE mirror::Object* GetReference(size_t i) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE Handle<mirror::Object> GetHandle(size_t i);

  ALWAYS_INLINE MutableHandle<mirror::Object> GetMutableHandle(size_t i)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetReference(size_t i, mirror::Object* object)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE bool Contains(StackReference<mirror::Object>* handle_scope_entry) const;

  // Offset of link within HandleScope, used by generated code.
  static constexpr size_t LinkOffset(PointerSize pointer_size ATTRIBUTE_UNUSED) {
    return 0;
  }

  // Offset of length within handle scope, used by generated code.
  static constexpr size_t NumberOfReferencesOffset(PointerSize pointer_size) {
    return static_cast<size_t>(pointer_size);
  }

  // Offset of link within handle scope, used by generated code.
  static constexpr size_t ReferencesOffset(PointerSize pointer_size) {
    return NumberOfReferencesOffset(pointer_size) + sizeof(number_of_references_);
  }

  // Placement new creation.
  static HandleScope* Create(void* storage, BaseHandleScope* link, uint32_t num_references)
      WARN_UNUSED {
    return new (storage) HandleScope(link, num_references);
  }

  // Number of references contained within this handle scope.
  ALWAYS_INLINE uint32_t NumberOfReferences() const {
    DCHECK_GE(number_of_references_, 0);
    return static_cast<uint32_t>(number_of_references_);
  }

  template <typename Visitor>
  void VisitRoots(Visitor& visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0, count = NumberOfReferences(); i < count; ++i) {
      // GetReference returns a pointer to the stack reference within the handle scope. If this
      // needs to be updated, it will be done by the root visitor.
      visitor.VisitRootIfNonNull(GetHandle(i).GetReference());
    }
  }

 protected:
  // Return backing storage used for references.
  ALWAYS_INLINE StackReference<mirror::Object>* GetReferences() const {
    uintptr_t address = reinterpret_cast<uintptr_t>(this) + ReferencesOffset(kRuntimePointerSize);
    return reinterpret_cast<StackReference<mirror::Object>*>(address);
  }

  explicit HandleScope(size_t number_of_references) : HandleScope(nullptr, number_of_references) {}

  // Semi-hidden constructor. Construction expected by generated code and StackHandleScope.
  HandleScope(BaseHandleScope* link, uint32_t num_references)
      : BaseHandleScope(link, num_references) {}

  // Storage for references.
  // StackReference<mirror::Object> references_[number_of_references_]

 private:
  DISALLOW_COPY_AND_ASSIGN(HandleScope);
};

// A wrapper which wraps around Object** and restores the pointer in the destructor.
// TODO: Delete
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


// A wrapper which wraps around ObjPtr<Object>* and restores the pointer in the destructor.
// TODO: Add more functionality.
template<class T>
class HandleWrapperObjPtr : public MutableHandle<T> {
 public:
  HandleWrapperObjPtr(ObjPtr<T>* obj, const MutableHandle<T>& handle)
      : MutableHandle<T>(handle), obj_(obj) {}

  HandleWrapperObjPtr(const HandleWrapperObjPtr&) = default;

  ~HandleWrapperObjPtr() {
    *obj_ = ObjPtr<T>(MutableHandle<T>::Get());
  }

 private:
  ObjPtr<T>* const obj_;
};

// Fixed size handle scope that is not necessarily linked in the thread.
template<size_t kNumReferences>
class PACKED(4) FixedSizeHandleScope : public HandleScope {
 public:
  template<class T>
  ALWAYS_INLINE MutableHandle<T> NewHandle(T* object) REQUIRES_SHARED(Locks::mutator_lock_);

  template<class T>
  ALWAYS_INLINE HandleWrapper<T> NewHandleWrapper(T** object)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<class T>
  ALWAYS_INLINE HandleWrapperObjPtr<T> NewHandleWrapper(ObjPtr<T>* object)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<class MirrorType>
  ALWAYS_INLINE MutableHandle<MirrorType> NewHandle(ObjPtr<MirrorType> object)
    REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetReference(size_t i, mirror::Object* object)
      REQUIRES_SHARED(Locks::mutator_lock_);

  size_t RemainingSlots() const {
    return kNumReferences - pos_;
  }

 private:
  explicit ALWAYS_INLINE FixedSizeHandleScope(BaseHandleScope* link,
                                              mirror::Object* fill_value = nullptr);
  ALWAYS_INLINE ~FixedSizeHandleScope() {}

  template<class T>
  ALWAYS_INLINE MutableHandle<T> GetHandle(size_t i) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_LT(i, kNumReferences);
    return MutableHandle<T>(&GetReferences()[i]);
  }

  // Reference storage needs to be first as expected by the HandleScope layout.
  StackReference<mirror::Object> storage_[kNumReferences];

  // Position new handles will be created.
  uint32_t pos_ = 0;

  template<size_t kNumRefs> friend class StackHandleScope;
  friend class VariableSizedHandleScope;
};

// Scoped handle storage of a fixed size that is stack allocated.
template<size_t kNumReferences>
class PACKED(4) StackHandleScope FINAL : public FixedSizeHandleScope<kNumReferences> {
 public:
  explicit ALWAYS_INLINE StackHandleScope(Thread* self, mirror::Object* fill_value = nullptr);
  ALWAYS_INLINE ~StackHandleScope();

  Thread* Self() const {
    return self_;
  }

 private:
  // The thread that the stack handle scope is a linked list upon. The stack handle scope will
  // push and pop itself from this thread.
  Thread* const self_;
};

// Utility class to manage a variable sized handle scope by having a list of fixed size handle
// scopes.
// Calls to NewHandle will create a new handle inside the current FixedSizeHandleScope.
// When the current handle scope becomes full a new one is created and put at the front of the
// list.
class VariableSizedHandleScope : public BaseHandleScope {
 public:
  explicit VariableSizedHandleScope(Thread* const self);
  ~VariableSizedHandleScope();

  template<class T>
  MutableHandle<T> NewHandle(T* object) REQUIRES_SHARED(Locks::mutator_lock_);

  template<class MirrorType>
  MutableHandle<MirrorType> NewHandle(ObjPtr<MirrorType> ptr)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Number of references contained within this handle scope.
  ALWAYS_INLINE uint32_t NumberOfReferences() const;

  ALWAYS_INLINE bool Contains(StackReference<mirror::Object>* handle_scope_entry) const;

  template <typename Visitor>
  void VisitRoots(Visitor& visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static constexpr size_t kLocalScopeSize = 64u;
  static constexpr size_t kSizeOfReferencesPerScope =
      kLocalScopeSize
          - /* BaseHandleScope::link_ */ sizeof(BaseHandleScope*)
          - /* BaseHandleScope::number_of_references_ */ sizeof(int32_t)
          - /* FixedSizeHandleScope<>::pos_ */ sizeof(uint32_t);
  static constexpr size_t kNumReferencesPerScope =
      kSizeOfReferencesPerScope / sizeof(StackReference<mirror::Object>);

  Thread* const self_;

  // Linked list of fixed size handle scopes.
  using LocalScopeType = FixedSizeHandleScope<kNumReferencesPerScope>;
  static_assert(sizeof(LocalScopeType) == kLocalScopeSize, "Unexpected size of LocalScopeType");
  LocalScopeType* current_scope_;

  DISALLOW_COPY_AND_ASSIGN(VariableSizedHandleScope);
};

}  // namespace art

#endif  // ART_RUNTIME_HANDLE_SCOPE_H_
