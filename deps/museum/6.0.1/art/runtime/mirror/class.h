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

#ifndef ART_RUNTIME_MIRROR_CLASS_H_
#define ART_RUNTIME_MIRROR_CLASS_H_

#include "base/iteration_range.h"
#include "dex_file.h"
#include "gc_root.h"
#include "gc/allocator_type.h"
#include "invoke_type.h"
#include "modifiers.h"
#include "object.h"
#include "object_array.h"
#include "object_callbacks.h"
#include "primitive.h"
#include "read_barrier_option.h"
#include "stride_iterator.h"
#include "utils.h"

#ifndef IMT_SIZE
#error IMT_SIZE not defined
#endif

namespace art {

class ArtField;
class ArtMethod;
struct ClassOffsets;
template<class T> class Handle;
template<class T> class Handle;
class Signature;
class StringPiece;
template<size_t kNumReferences> class PACKED(4) StackHandleScope;

namespace mirror {

class ClassLoader;
class Constructor;
class DexCache;
class IfTable;

// C++ mirror of java.lang.Class
class MANAGED Class FINAL : public Object {
 public:
  // A magic value for reference_instance_offsets_. Ignore the bits and walk the super chain when
  // this is the value.
  // [This is an unlikely "natural" value, since it would be 30 non-ref instance fields followed by
  // 2 ref instance fields.]
  static constexpr uint32_t kClassWalkSuper = 0xC0000000;

  // Interface method table size. Increasing this value reduces the chance of two interface methods
  // colliding in the interface method table but increases the size of classes that implement
  // (non-marker) interfaces.
  static constexpr size_t kImtSize = IMT_SIZE;

  // Class Status
  //
  // kStatusRetired: Class that's temporarily used till class linking time
  // has its (vtable) size figured out and has been cloned to one with the
  // right size which will be the one used later. The old one is retired and
  // will be gc'ed once all refs to the class point to the newly
  // cloned version.
  //
  // kStatusNotReady: If a Class cannot be found in the class table by
  // FindClass, it allocates an new one with AllocClass in the
  // kStatusNotReady and calls LoadClass. Note if it does find a
  // class, it may not be kStatusResolved and it will try to push it
  // forward toward kStatusResolved.
  //
  // kStatusIdx: LoadClass populates with Class with information from
  // the DexFile, moving the status to kStatusIdx, indicating that the
  // Class value in super_class_ has not been populated. The new Class
  // can then be inserted into the classes table.
  //
  // kStatusLoaded: After taking a lock on Class, the ClassLinker will
  // attempt to move a kStatusIdx class forward to kStatusLoaded by
  // using ResolveClass to initialize the super_class_ and ensuring the
  // interfaces are resolved.
  //
  // kStatusResolving: Class is just cloned with the right size from
  // temporary class that's acting as a placeholder for linking. The old
  // class will be retired. New class is set to this status first before
  // moving on to being resolved.
  //
  // kStatusResolved: Still holding the lock on Class, the ClassLinker
  // shows linking is complete and fields of the Class populated by making
  // it kStatusResolved. Java allows circularities of the form where a super
  // class has a field that is of the type of the sub class. We need to be able
  // to fully resolve super classes while resolving types for fields.
  //
  // kStatusRetryVerificationAtRuntime: The verifier sets a class to
  // this state if it encounters a soft failure at compile time. This
  // often happens when there are unresolved classes in other dex
  // files, and this status marks a class as needing to be verified
  // again at runtime.
  //
  // TODO: Explain the other states
  enum Status {
    kStatusRetired = -2,  // Retired, should not be used. Use the newly cloned one instead.
    kStatusError = -1,
    kStatusNotReady = 0,
    kStatusIdx = 1,  // Loaded, DEX idx in super_class_type_idx_ and interfaces_type_idx_.
    kStatusLoaded = 2,  // DEX idx values resolved.
    kStatusResolving = 3,  // Just cloned from temporary class object.
    kStatusResolved = 4,  // Part of linking.
    kStatusVerifying = 5,  // In the process of being verified.
    kStatusRetryVerificationAtRuntime = 6,  // Compile time verification failed, retry at runtime.
    kStatusVerifyingAtRuntime = 7,  // Retrying verification at runtime.
    kStatusVerified = 8,  // Logically part of linking; done pre-init.
    kStatusInitializing = 9,  // Class init in progress.
    kStatusInitialized = 10,  // Ready to go.
    kStatusMax = 11,
  };

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  Status GetStatus() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    static_assert(sizeof(Status) == sizeof(uint32_t), "Size of status not equal to uint32");
    return static_cast<Status>(
        GetField32Volatile<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, status_)));
  }

  // This is static because 'this' may be moved by GC.
  static void SetStatus(Handle<Class> h_this, Status new_status, Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset StatusOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, status_);
  }

  // Returns true if the class has been retired.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsRetired() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == kStatusRetired;
  }

  // Returns true if the class has failed to link.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsErroneous() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == kStatusError;
  }

  // Returns true if the class has been loaded.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsIdxLoaded() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= kStatusIdx;
  }

  // Returns true if the class has been loaded.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsLoaded() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= kStatusLoaded;
  }

  // Returns true if the class has been linked.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsResolved() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= kStatusResolved;
  }

  // Returns true if the class was compile-time verified.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsCompileTimeVerified() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= kStatusRetryVerificationAtRuntime;
  }

  // Returns true if the class has been verified.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsVerified() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= kStatusVerified;
  }

  // Returns true if the class is initializing.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsInitializing() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= kStatusInitializing;
  }

  // Returns true if the class is initialized.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsInitialized() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == kStatusInitialized;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint32_t GetAccessFlags() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static MemberOffset AccessFlagsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, access_flags_);
  }

  void SetAccessFlags(uint32_t new_access_flags) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns true if the class is an interface.
  ALWAYS_INLINE bool IsInterface() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccInterface) != 0;
  }

  // Returns true if the class is declared public.
  ALWAYS_INLINE bool IsPublic() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  // Returns true if the class is declared final.
  ALWAYS_INLINE bool IsFinal() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  ALWAYS_INLINE bool IsFinalizable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccClassIsFinalizable) != 0;
  }

  ALWAYS_INLINE void SetFinalizable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    SetAccessFlags(flags | kAccClassIsFinalizable);
  }

  ALWAYS_INLINE bool IsStringClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetField32(AccessFlagsOffset()) & kAccClassIsStringClass) != 0;
  }

  ALWAYS_INLINE void SetStringClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    SetAccessFlags(flags | kAccClassIsStringClass);
  }

  // Returns true if the class is abstract.
  ALWAYS_INLINE bool IsAbstract() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccAbstract) != 0;
  }

  // Returns true if the class is an annotation.
  ALWAYS_INLINE bool IsAnnotation() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccAnnotation) != 0;
  }

  // Returns true if the class is synthetic.
  ALWAYS_INLINE bool IsSynthetic() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccSynthetic) != 0;
  }

  // Returns true if the class can avoid access checks.
  bool IsPreverified() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccPreverified) != 0;
  }

  void SetPreverified() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    SetAccessFlags(flags | kAccPreverified);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsTypeOfReferenceClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccClassIsReference) != 0;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsWeakReferenceClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccClassIsWeakReference) != 0;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsSoftReferenceClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccReferenceFlagsMask) == kAccClassIsReference;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsFinalizerReferenceClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccClassIsFinalizerReference) != 0;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPhantomReferenceClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccClassIsPhantomReference) != 0;
  }

  // Can references of this type be assigned to by things of another type? For non-array types
  // this is a matter of whether sub-classes may exist - which they can't if the type is final.
  // For array classes, where all the classes are final due to there being no sub-classes, an
  // Object[] may be assigned to by a String[] but a String[] may not be assigned to by other
  // types as the component is final.
  bool CannotBeAssignedFromOtherTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsArrayClass()) {
      return IsFinal();
    } else {
      Class* component = GetComponentType();
      if (component->IsPrimitive()) {
        return true;
      } else {
        return component->CannotBeAssignedFromOtherTypes();
      }
    }
  }

  // Returns true if this class is the placeholder and should retire and
  // be replaced with a class with the right size for embedded imt/vtable.
  bool IsTemp() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    Status s = GetStatus();
    return s < Status::kStatusResolving && ShouldHaveEmbeddedImtAndVTable();
  }

  String* GetName() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);  // Returns the cached name.
  void SetName(String* name) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);  // Sets the cached name.
  // Computes the name, then sets the cached value.
  static String* ComputeName(Handle<Class> h_this) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsProxyClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Read access flags without using getter as whether something is a proxy can be check in
    // any loaded state
    // TODO: switch to a check if the super class is java.lang.reflect.Proxy?
    uint32_t access_flags = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    return (access_flags & kAccClassIsProxy) != 0;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  Primitive::Type GetPrimitiveType() ALWAYS_INLINE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetPrimitiveType(Primitive::Type new_type) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK_EQ(sizeof(Primitive::Type), sizeof(int32_t));
    int32_t v32 = static_cast<int32_t>(new_type);
    DCHECK_EQ(v32 & 0xFFFF, v32) << "upper 16 bits aren't zero";
    // Store the component size shift in the upper 16 bits.
    v32 |= Primitive::ComponentSizeShift(new_type) << 16;
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_), v32);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t GetPrimitiveTypeSizeShift() ALWAYS_INLINE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns true if the class is a primitive type.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitive() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() != Primitive::kPrimNot;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveBoolean() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimBoolean;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveByte() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimByte;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveChar() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimChar;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveShort() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimShort;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveInt() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType() == Primitive::kPrimInt;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveLong() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimLong;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveFloat() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimFloat;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveDouble() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimDouble;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveVoid() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimVoid;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveArray() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return IsArrayClass<kVerifyFlags>() &&
        GetComponentType<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>()->
        IsPrimitive();
  }

  // Depth of class from java.lang.Object
  uint32_t Depth() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsArrayClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetComponentType<kVerifyFlags, kReadBarrierOption>() != nullptr;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsClassClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsThrowableClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsReferenceClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset ComponentTypeOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, component_type_);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  Class* GetComponentType() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(ComponentTypeOffset());
  }

  void SetComponentType(Class* new_component_type) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(GetComponentType() == nullptr);
    DCHECK(new_component_type != nullptr);
    // Component type is invariant: use non-transactional mode without check.
    SetFieldObject<false, false>(ComponentTypeOffset(), new_component_type);
  }

  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  size_t GetComponentSize() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return 1U << GetComponentSizeShift();
  }

  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  size_t GetComponentSizeShift() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetComponentType<kDefaultVerifyFlags, kReadBarrierOption>()->GetPrimitiveTypeSizeShift();
  }

  bool IsObjectClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return !IsPrimitive() && GetSuperClass() == nullptr;
  }

  bool IsInstantiableNonArray() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return !IsPrimitive() && !IsInterface() && !IsAbstract() && !IsArrayClass();
  }

  bool IsInstantiable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return (!IsPrimitive() && !IsInterface() && !IsAbstract()) ||
        (IsAbstract() && IsArrayClass());
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsObjectArrayClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetComponentType<kVerifyFlags>() != nullptr &&
        !GetComponentType<kVerifyFlags>()->IsPrimitive();
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsIntArrayClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
    auto* component_type = GetComponentType<kVerifyFlags>();
    return component_type != nullptr && component_type->template IsPrimitiveInt<kNewFlags>();
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsLongArrayClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
    auto* component_type = GetComponentType<kVerifyFlags>();
    return component_type != nullptr && component_type->template IsPrimitiveLong<kNewFlags>();
  }

  // Creates a raw object instance but does not invoke the default constructor.
  template<bool kIsInstrumented, bool kCheckAddFinalizer = true>
  ALWAYS_INLINE Object* Alloc(Thread* self, gc::AllocatorType allocator_type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Object* AllocObject(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  Object* AllocNonMovableObject(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsVariableSize() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Classes, arrays, and strings vary in size, and so the object_size_ field cannot
    // be used to Get their instance size
    return IsClassClass<kVerifyFlags, kReadBarrierOption>() ||
        IsArrayClass<kVerifyFlags, kReadBarrierOption>() || IsStringClass();
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  uint32_t SizeOf() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, class_size_));
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t GetClassSize() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, class_size_));
  }

  void SetClassSize(uint32_t new_class_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Compute how many bytes would be used a class with the given elements.
  static uint32_t ComputeClassSize(bool has_embedded_tables,
                                   uint32_t num_vtable_entries,
                                   uint32_t num_8bit_static_fields,
                                   uint32_t num_16bit_static_fields,
                                   uint32_t num_32bit_static_fields,
                                   uint32_t num_64bit_static_fields,
                                   uint32_t num_ref_static_fields,
                                   size_t pointer_size);

  // The size of java.lang.Class.class.
  static uint32_t ClassClassSize(size_t pointer_size) {
    // The number of vtable entries in java.lang.Class.
    uint32_t vtable_entries = Object::kVTableLength + 65;
    return ComputeClassSize(true, vtable_entries, 0, 0, 0, 1, 0, pointer_size);
  }

  // The size of a java.lang.Class representing a primitive such as int.class.
  static uint32_t PrimitiveClassSize(size_t pointer_size) {
    return ComputeClassSize(false, 0, 0, 0, 0, 0, 0, pointer_size);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  uint32_t GetObjectSize() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static MemberOffset ObjectSizeOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, object_size_);
  }

  void SetObjectSize(uint32_t new_object_size) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(!IsVariableSize());
    // Not called within a transaction.
    return SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, object_size_), new_object_size);
  }

  void SetObjectSizeWithoutChecks(uint32_t new_object_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Not called within a transaction.
    return SetField32<false, false, kVerifyNone>(
        OFFSET_OF_OBJECT_MEMBER(Class, object_size_), new_object_size);
  }

  // Returns true if this class is in the same packages as that class.
  bool IsInSamePackage(Class* that) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsInSamePackage(const StringPiece& descriptor1, const StringPiece& descriptor2);

  // Returns true if this class can access that class.
  bool CanAccess(Class* that) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return that->IsPublic() || this->IsInSamePackage(that);
  }

  // Can this class access a member in the provided class with the provided member access flags?
  // Note that access to the class isn't checked in case the declaring class is protected and the
  // method has been exposed by a public sub-class
  bool CanAccessMember(Class* access_to, uint32_t member_flags)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Classes can access all of their own members
    if (this == access_to) {
      return true;
    }
    // Public members are trivially accessible
    if (member_flags & kAccPublic) {
      return true;
    }
    // Private members are trivially not accessible
    if (member_flags & kAccPrivate) {
      return false;
    }
    // Check for protected access from a sub-class, which may or may not be in the same package.
    if (member_flags & kAccProtected) {
      if (!this->IsInterface() && this->IsSubClass(access_to)) {
        return true;
      }
    }
    // Allow protected access from other classes in the same package.
    return this->IsInSamePackage(access_to);
  }

  // Can this class access a resolved field?
  // Note that access to field's class is checked and this may require looking up the class
  // referenced by the FieldId in the DexFile in case the declaring class is inaccessible.
  bool CanAccessResolvedField(Class* access_to, ArtField* field,
                              DexCache* dex_cache, uint32_t field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool CheckResolvedFieldAccess(Class* access_to, ArtField* field,
                                uint32_t field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this class access a resolved method?
  // Note that access to methods's class is checked and this may require looking up the class
  // referenced by the MethodId in the DexFile in case the declaring class is inaccessible.
  bool CanAccessResolvedMethod(Class* access_to, ArtMethod* resolved_method,
                               DexCache* dex_cache, uint32_t method_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  template <InvokeType throw_invoke_type>
  bool CheckResolvedMethodAccess(Class* access_to, ArtMethod* resolved_method,
                                 uint32_t method_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsSubClass(Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can src be assigned to this class? For example, String can be assigned to Object (by an
  // upcast), however, an Object cannot be assigned to a String as a potentially exception throwing
  // downcast would be necessary. Similarly for interfaces, a class that implements (or an interface
  // that extends) another can be assigned to its parent, but not vice-versa. All Classes may assign
  // to themselves. Classes for primitive types may not assign to each other.
  ALWAYS_INLINE bool IsAssignableFrom(Class* src) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(src != nullptr);
    if (this == src) {
      // Can always assign to things of the same type.
      return true;
    } else if (IsObjectClass()) {
      // Can assign any reference to java.lang.Object.
      return !src->IsPrimitive();
    } else if (IsInterface()) {
      return src->Implements(this);
    } else if (src->IsArrayClass()) {
      return IsAssignableFromArray(src);
    } else {
      return !src->IsInterface() && src->IsSubClass(this);
    }
  }

  ALWAYS_INLINE Class* GetSuperClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSuperClass(Class *new_super_class) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Super class is assigned once, except during class linker initialization.
    Class* old_super_class = GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(Class, super_class_));
    DCHECK(old_super_class == nullptr || old_super_class == new_super_class);
    DCHECK(new_super_class != nullptr);
    SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(Class, super_class_), new_super_class);
  }

  bool HasSuperClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetSuperClass() != nullptr;
  }

  static MemberOffset SuperClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, super_class_));
  }

  ClassLoader* GetClassLoader() ALWAYS_INLINE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetClassLoader(ClassLoader* new_cl) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset DexCacheOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, dex_cache_));
  }

  enum {
    kDumpClassFullDetail = 1,
    kDumpClassClassLoader = (1 << 1),
    kDumpClassInitialized = (1 << 2),
  };

  void DumpClass(std::ostream& os, int flags) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  DexCache* GetDexCache() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Also updates the dex_cache_strings_ variable from new_dex_cache.
  void SetDexCache(DexCache* new_dex_cache) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE StrideIterator<ArtMethod> DirectMethodsBegin(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE StrideIterator<ArtMethod> DirectMethodsEnd(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE IterationRange<StrideIterator<ArtMethod>> GetDirectMethods(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetDirectMethodsPtr() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);\

  void SetDirectMethodsPtr(ArtMethod* new_direct_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Used by image writer.
  void SetDirectMethodsPtrUnchecked(ArtMethod* new_direct_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetDirectMethod(size_t i, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Use only when we are allocating populating the method arrays.
  ALWAYS_INLINE ArtMethod* GetDirectMethodUnchecked(size_t i, size_t pointer_size)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ALWAYS_INLINE ArtMethod* GetVirtualMethodUnchecked(size_t i, size_t pointer_size)
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of static, private, and constructor methods.
  ALWAYS_INLINE uint32_t NumDirectMethods() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_direct_methods_));
  }
  void SetNumDirectMethods(uint32_t num) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_direct_methods_), num);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArtMethod* GetVirtualMethodsPtr() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE StrideIterator<ArtMethod> VirtualMethodsBegin(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE StrideIterator<ArtMethod> VirtualMethodsEnd(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE IterationRange<StrideIterator<ArtMethod>> GetVirtualMethods(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetVirtualMethodsPtr(ArtMethod* new_virtual_methods)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of non-inherited virtual methods.
  ALWAYS_INLINE uint32_t NumVirtualMethods() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_virtual_methods_));
  }
  void SetNumVirtualMethods(uint32_t num) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_virtual_methods_), num);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ArtMethod* GetVirtualMethod(size_t i, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetVirtualMethodDuringLinking(size_t i, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE PointerArray* GetVTable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE PointerArray* GetVTableDuringLinking() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetVTable(PointerArray* new_vtable) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset VTableOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, vtable_);
  }

  static MemberOffset EmbeddedVTableLengthOffset() {
    return MemberOffset(sizeof(Class));
  }

  bool ShouldHaveEmbeddedImtAndVTable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return IsInstantiable();
  }

  bool HasVTable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static MemberOffset EmbeddedImTableEntryOffset(uint32_t i, size_t pointer_size);

  static MemberOffset EmbeddedVTableEntryOffset(uint32_t i, size_t pointer_size);

  ArtMethod* GetEmbeddedImTableEntry(uint32_t i, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetEmbeddedImTableEntry(uint32_t i, ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t GetVTableLength() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetVTableEntry(uint32_t i, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  int32_t GetEmbeddedVTableLength() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetEmbeddedVTableLength(int32_t len) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* GetEmbeddedVTableEntry(uint32_t i, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetEmbeddedVTableEntry(uint32_t i, ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  inline void SetEmbeddedVTableEntryUnchecked(uint32_t i, ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void PopulateEmbeddedImtAndVTable(ArtMethod* const (&methods)[kImtSize], size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Given a method implemented by this class but potentially from a super class, return the
  // specific implementation method for this class.
  ArtMethod* FindVirtualMethodForVirtual(ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Given a method implemented by this class' super class, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForSuper(ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Given a method implemented by this class, but potentially from a
  // super class or interface, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForInterface(ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) ALWAYS_INLINE;

  ArtMethod* FindVirtualMethodForVirtualOrInterface(ArtMethod* method, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindInterfaceMethod(const StringPiece& name, const StringPiece& signature,
                                 size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindInterfaceMethod(const StringPiece& name, const Signature& signature,
                                 size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindInterfaceMethod(const DexCache* dex_cache, uint32_t dex_method_idx,
                                 size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredDirectMethod(const StringPiece& name, const StringPiece& signature,
                                      size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredDirectMethod(const StringPiece& name, const Signature& signature,
                                      size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredDirectMethod(const DexCache* dex_cache, uint32_t dex_method_idx,
                                      size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDirectMethod(const StringPiece& name, const StringPiece& signature,
                              size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDirectMethod(const StringPiece& name, const Signature& signature,
                              size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDirectMethod(const DexCache* dex_cache, uint32_t dex_method_idx,
                              size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredVirtualMethod(const StringPiece& name, const StringPiece& signature,
                                       size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredVirtualMethod(const StringPiece& name, const Signature& signature,
                                       size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx,
                                       size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindVirtualMethod(const StringPiece& name, const StringPiece& signature,
                               size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindVirtualMethod(const StringPiece& name, const Signature& signature,
                               size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx,
                               size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtMethod* FindClassInitializer(size_t pointer_size) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE int32_t GetIfTableCount() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE IfTable* GetIfTable() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetIfTable(IfTable* new_iftable) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get instance fields of the class (See also GetSFields).
  ArtField* GetIFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetIFields(ArtField* new_ifields) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Unchecked edition has no verification flags.
  void SetIFieldsUnchecked(ArtField* new_sfields) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint32_t NumInstanceFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_instance_fields_));
  }

  void SetNumInstanceFields(uint32_t num) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_instance_fields_), num);
  }

  ArtField* GetInstanceField(uint32_t i) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of instance fields containing reference types.
  uint32_t NumReferenceInstanceFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(IsResolved() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_));
  }

  uint32_t NumReferenceInstanceFieldsDuringLinking() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(IsLoaded() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_));
  }

  void SetNumReferenceInstanceFields(uint32_t new_num) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Not called within a transaction.
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_), new_num);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t GetReferenceInstanceOffsets() ALWAYS_INLINE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetReferenceInstanceOffsets(uint32_t new_reference_offsets)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the offset of the first reference instance field. Other reference instance fields follow.
  MemberOffset GetFirstReferenceInstanceFieldOffset()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Returns the number of static fields containing reference types.
  uint32_t NumReferenceStaticFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(IsResolved() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_));
  }

  uint32_t NumReferenceStaticFieldsDuringLinking() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(IsLoaded() || IsErroneous() || IsRetired());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_));
  }

  void SetNumReferenceStaticFields(uint32_t new_num) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Not called within a transaction.
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_), new_num);
  }

  // Get the offset of the first reference static field. Other reference static fields follow.
  MemberOffset GetFirstReferenceStaticFieldOffset(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the offset of the first reference static field. Other reference static fields follow.
  MemberOffset GetFirstReferenceStaticFieldOffsetDuringLinking(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Gets the static fields of the class.
  ArtField* GetSFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetSFields(ArtField* new_sfields) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Unchecked edition has no verification flags.
  void SetSFieldsUnchecked(ArtField* new_sfields) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint32_t NumStaticFields() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_static_fields_));
  }

  void SetNumStaticFields(uint32_t num) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_static_fields_), num);
  }

  // TODO: uint16_t
  ArtField* GetStaticField(uint32_t i) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Find a static or instance field using the JLS resolution order
  static ArtField* FindField(Thread* self, Handle<Class> klass, const StringPiece& name,
                             const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given instance field in this class or a superclass.
  ArtField* FindInstanceField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given instance field in this class or a superclass, only searches classes that
  // have the same dex cache.
  ArtField* FindInstanceField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredInstanceField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredInstanceField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given static field in this class or a superclass.
  static ArtField* FindStaticField(Thread* self, Handle<Class> klass, const StringPiece& name,
                                   const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Finds the given static field in this class or superclass, only searches classes that
  // have the same dex cache.
  static ArtField* FindStaticField(Thread* self, Handle<Class> klass, const DexCache* dex_cache,
                                   uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredStaticField(const StringPiece& name, const StringPiece& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ArtField* FindDeclaredStaticField(const DexCache* dex_cache, uint32_t dex_field_idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  pid_t GetClinitThreadId() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(IsIdxLoaded() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_));
  }

  void SetClinitThreadId(pid_t new_clinit_thread_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Class* GetVerifyErrorClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // DCHECK(IsErroneous());
    return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(Class, verify_error_class_));
  }

  uint16_t GetDexClassDefIndex() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_class_def_idx_));
  }

  void SetDexClassDefIndex(uint16_t class_def_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Not called within a transaction.
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, dex_class_def_idx_), class_def_idx);
  }

  uint16_t GetDexTypeIndex() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_type_idx_));
  }

  void SetDexTypeIndex(uint16_t type_idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // Not called within a transaction.
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, dex_type_idx_), type_idx);
  }

  static Class* GetJavaLangClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(HasJavaLangClass());
    return java_lang_Class_.Read();
  }

  static bool HasJavaLangClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return !java_lang_Class_.IsNull();
  }

  // Can't call this SetClass or else gets called instead of Object::SetClass in places.
  static void SetClassClass(Class* java_lang_Class) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void ResetClass();
  static void VisitRoots(RootVisitor* visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Visit native roots visits roots which are keyed off the native pointers such as ArtFields and
  // ArtMethods.
  template<class Visitor>
  void VisitNativeRoots(Visitor& visitor, size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // When class is verified, set the kAccPreverified flag on each method.
  void SetPreverifiedFlagOnAllMethods(size_t pointer_size)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template <bool kVisitClass, typename Visitor>
  void VisitReferences(mirror::Class* klass, const Visitor& visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the descriptor of the class. In a few cases a std::string is required, rather than
  // always create one the storage argument is populated and its internal c_str() returned. We do
  // this to avoid memory allocation in the common case.
  const char* GetDescriptor(std::string* storage) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const char* GetArrayDescriptor(std::string* storage) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool DescriptorEquals(const char* match) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const DexFile::ClassDef* GetClassDef() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE uint32_t NumDirectInterfaces() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  uint16_t GetDirectInterfaceTypeIdx(uint32_t idx) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static mirror::Class* GetDirectInterface(Thread* self, Handle<mirror::Class> klass,
                                           uint32_t idx)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const char* GetSourceFile() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  std::string GetLocation() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const DexFile& GetDexFile() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const DexFile::TypeList* GetInterfaceTypeList() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Asserts we are initialized or initializing in the given thread.
  void AssertInitializedOrInitializingInThread(Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Class* CopyOf(Thread* self, int32_t new_length, ArtMethod* const (&imt)[mirror::Class::kImtSize],
                size_t pointer_size) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // For proxy class only.
  ObjectArray<Class>* GetInterfaces() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // For proxy class only.
  ObjectArray<ObjectArray<Class>>* GetThrows() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // For reference class only.
  MemberOffset GetDisableIntrinsicFlagOffset() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  MemberOffset GetSlowPathFlagOffset() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool GetSlowPathEnabled() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetSlowPath(bool enabled) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ObjectArray<String>* GetDexCacheStrings() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void SetDexCacheStrings(ObjectArray<String>* new_dex_cache_strings)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static MemberOffset DexCacheStringsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_strings_);
  }

  // May cause thread suspension due to EqualParameters.
  ArtMethod* GetDeclaredConstructor(
      Thread* self, Handle<mirror::ObjectArray<mirror::Class>> args)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Used to initialize a class in the allocation code path to ensure it is guarded by a StoreStore
  // fence.
  class InitializeClassVisitor {
   public:
    explicit InitializeClassVisitor(uint32_t class_size) : class_size_(class_size) {
    }

    void operator()(mirror::Object* obj, size_t usable_size) const
        SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

   private:
    const uint32_t class_size_;

    DISALLOW_COPY_AND_ASSIGN(InitializeClassVisitor);
  };

  // Returns true if the class loader is null, ie the class loader is the boot strap class loader.
  bool IsBootStrapClassLoaded() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return GetClassLoader() == nullptr;
  }

  static size_t ImTableEntrySize(size_t pointer_size) {
    return pointer_size;
  }

  static size_t VTableEntrySize(size_t pointer_size) {
    return pointer_size;
  }

  ALWAYS_INLINE ArtMethod* GetDirectMethodsPtrUnchecked()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetVirtualMethodsPtrUnchecked()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void SetVerifyErrorClass(Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  template <bool throw_on_failure, bool use_referrers_cache>
  bool ResolvedFieldAccessTest(Class* access_to, ArtField* field,
                               uint32_t field_idx, DexCache* dex_cache)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  template <bool throw_on_failure, bool use_referrers_cache, InvokeType throw_invoke_type>
  bool ResolvedMethodAccessTest(Class* access_to, ArtMethod* resolved_method,
                                uint32_t method_idx, DexCache* dex_cache)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool Implements(Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsArrayAssignableFromArray(Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsAssignableFromArray(Class* klass) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void CheckObjectAlloc() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Unchecked editions is for root visiting.
  ArtField* GetSFieldsUnchecked() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ArtField* GetIFieldsUnchecked() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool ProxyDescriptorEquals(const char* match) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Check that the pointer size mathces the one in the class linker.
  ALWAYS_INLINE static void CheckPointerSize(size_t pointer_size);

  static MemberOffset EmbeddedImTableOffset(size_t pointer_size);
  static MemberOffset EmbeddedVTableOffset(size_t pointer_size);

  // Defining class loader, or null for the "bootstrap" system loader.
  HeapReference<ClassLoader> class_loader_;

  // For array classes, the component class object for instanceof/checkcast
  // (for String[][][], this will be String[][]). null for non-array classes.
  HeapReference<Class> component_type_;

  // DexCache of resolved constant pool entries (will be null for classes generated by the
  // runtime such as arrays and primitive classes).
  HeapReference<DexCache> dex_cache_;

  // Short cuts to dex_cache_ member for fast compiled code access.
  HeapReference<ObjectArray<String>> dex_cache_strings_;

  // The interface table (iftable_) contains pairs of a interface class and an array of the
  // interface methods. There is one pair per interface supported by this class.  That means one
  // pair for each interface we support directly, indirectly via superclass, or indirectly via a
  // superinterface.  This will be null if neither we nor our superclass implement any interfaces.
  //
  // Why we need this: given "class Foo implements Face", declare "Face faceObj = new Foo()".
  // Invoke faceObj.blah(), where "blah" is part of the Face interface.  We can't easily use a
  // single vtable.
  //
  // For every interface a concrete class implements, we create an array of the concrete vtable_
  // methods for the methods in the interface.
  HeapReference<IfTable> iftable_;

  // Descriptor for the class such as "java.lang.Class" or "[C". Lazily initialized by ComputeName
  HeapReference<String> name_;

  // The superclass, or null if this is java.lang.Object, an interface or primitive type.
  HeapReference<Class> super_class_;

  // If class verify fails, we must return same error on subsequent tries.
  HeapReference<Class> verify_error_class_;

  // Virtual method table (vtable), for use by "invoke-virtual".  The vtable from the superclass is
  // copied in, and virtual methods from our class either replace those from the super or are
  // appended. For abstract classes, methods may be created in the vtable that aren't in
  // virtual_ methods_ for miranda methods.
  HeapReference<PointerArray> vtable_;

  // Access flags; low 16 bits are defined by VM spec.
  // Note: Shuffled back.
  uint32_t access_flags_;

  // static, private, and <init> methods. Pointer to an ArtMethod array.
  uint64_t direct_methods_;

  // instance fields
  //
  // These describe the layout of the contents of an Object.
  // Note that only the fields directly declared by this class are
  // listed in ifields; fields declared by a superclass are listed in
  // the superclass's Class.ifields.
  //
  // ArtField arrays are allocated as an array of fields, and not an array of fields pointers.
  uint64_t ifields_;

  // Static fields
  uint64_t sfields_;

  // Virtual methods defined in this class; invoked through vtable. Pointer to an ArtMethod array.
  uint64_t virtual_methods_;

  // Total size of the Class instance; used when allocating storage on gc heap.
  // See also object_size_.
  uint32_t class_size_;

  // Tid used to check for recursive <clinit> invocation.
  pid_t clinit_thread_id_;

  // ClassDef index in dex file, -1 if no class definition such as an array.
  // TODO: really 16bits
  int32_t dex_class_def_idx_;

  // Type index in dex file.
  // TODO: really 16bits
  int32_t dex_type_idx_;

  // Number of direct fields.
  uint32_t num_direct_methods_;

  // Number of instance fields.
  uint32_t num_instance_fields_;

  // Number of instance fields that are object refs.
  uint32_t num_reference_instance_fields_;

  // Number of static fields that are object refs,
  uint32_t num_reference_static_fields_;

  // Number of static fields.
  uint32_t num_static_fields_;

  // Number of virtual methods.
  uint32_t num_virtual_methods_;

  // Total object size; used when allocating storage on gc heap.
  // (For interfaces and abstract classes this will be zero.)
  // See also class_size_.
  uint32_t object_size_;

  // The lower 16 bits contains a Primitive::Type value. The upper 16
  // bits contains the size shift of the primitive type.
  uint32_t primitive_type_;

  // Bitmap of offsets of ifields.
  uint32_t reference_instance_offsets_;

  // State of class initialization.
  Status status_;

  // TODO: ?
  // initiating class loader list
  // NOTE: for classes with low serialNumber, these are unused, and the
  // values are kept in a table in gDvm.
  // InitiatingLoaderList initiating_loader_list_;

  // The following data exist in real class objects.
  // Embedded Imtable, for class object that's not an interface, fixed size.
  // ImTableEntry embedded_imtable_[0];
  // Embedded Vtable, for class object that's not an interface, variable size.
  // VTableEntry embedded_vtable_[0];
  // Static fields, variable size.
  // uint32_t fields_[0];

  // java.lang.Class
  static GcRoot<Class> java_lang_Class_;

  friend struct art::ClassOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(Class);
};

std::ostream& operator<<(std::ostream& os, const Class::Status& rhs);

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_H_
