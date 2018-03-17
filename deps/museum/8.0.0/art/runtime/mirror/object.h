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

#ifndef ART_RUNTIME_MIRROR_OBJECT_H_
#define ART_RUNTIME_MIRROR_OBJECT_H_

#include "atomic.h"
#include "base/casts.h"
#include "base/enums.h"
#include "globals.h"
#include "obj_ptr.h"
#include "object_reference.h"
#include "offsets.h"
#include "verify_object.h"

namespace art {

class ArtField;
class ArtMethod;
class ImageWriter;
class LockWord;
class Monitor;
struct ObjectOffsets;
class Thread;
class VoidFunctor;

namespace mirror {

class Array;
class Class;
class ClassLoader;
class DexCache;
class FinalizerReference;
template<class T> class ObjectArray;
template<class T> class PrimitiveArray;
typedef PrimitiveArray<uint8_t> BooleanArray;
typedef PrimitiveArray<int8_t> ByteArray;
typedef PrimitiveArray<uint16_t> CharArray;
typedef PrimitiveArray<double> DoubleArray;
typedef PrimitiveArray<float> FloatArray;
typedef PrimitiveArray<int32_t> IntArray;
typedef PrimitiveArray<int64_t> LongArray;
typedef PrimitiveArray<int16_t> ShortArray;
class Reference;
class String;
class Throwable;

// Fields within mirror objects aren't accessed directly so that the appropriate amount of
// handshaking is done with GC (for example, read and write barriers). This macro is used to
// compute an offset for the Set/Get methods defined in Object that can safely access fields.
#define OFFSET_OF_OBJECT_MEMBER(type, field) \
    MemberOffset(OFFSETOF_MEMBER(type, field))

// Checks that we don't do field assignments which violate the typing system.
static constexpr bool kCheckFieldAssignments = false;

// Size of Object.
static constexpr uint32_t kObjectHeaderSize = kUseBrooksReadBarrier ? 16 : 8;

// C++ mirror of java.lang.Object
class MANAGED LOCKABLE Object {
 public:
  // The number of vtable entries in java.lang.Object.
  static constexpr size_t kVTableLength = 11;

  // The size of the java.lang.Class representing a java.lang.Object.
  static uint32_t ClassSize(PointerSize pointer_size);

  // Size of an instance of java.lang.Object.
  static constexpr uint32_t InstanceSize() {
    return sizeof(Object);
  }

  static MemberOffset ClassOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Object, klass_);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE Class* GetClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetClass(ObjPtr<Class> new_klass) REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the read barrier state with a fake address dependency.
  // '*fake_address_dependency' will be set to 0.
  ALWAYS_INLINE uint32_t GetReadBarrierState(uintptr_t* fake_address_dependency)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // This version does not offer any special mechanism to prevent load-load reordering.
  ALWAYS_INLINE uint32_t GetReadBarrierState() REQUIRES_SHARED(Locks::mutator_lock_);
  // Get the read barrier state with a load-acquire.
  ALWAYS_INLINE uint32_t GetReadBarrierStateAcquire() REQUIRES_SHARED(Locks::mutator_lock_);

#ifndef USE_BAKER_OR_BROOKS_READ_BARRIER
  NO_RETURN
#endif
  ALWAYS_INLINE void SetReadBarrierState(uint32_t rb_state) REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kCasRelease = false>
  ALWAYS_INLINE bool AtomicSetReadBarrierState(uint32_t expected_rb_state, uint32_t rb_state)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE uint32_t GetMarkBit() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE bool AtomicSetMarkBit(uint32_t expected_mark_bit, uint32_t mark_bit)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Assert that the read barrier state is in the default (white) state.
  ALWAYS_INLINE void AssertReadBarrierState() const REQUIRES_SHARED(Locks::mutator_lock_);

  // The verifier treats all interfaces as java.lang.Object and relies on runtime checks in
  // invoke-interface to detect incompatible interface types.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool VerifierInstanceOf(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool InstanceOf(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t SizeOf() REQUIRES_SHARED(Locks::mutator_lock_);

  Object* Clone(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  int32_t IdentityHashCode()
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::thread_list_lock_,
               !Locks::thread_suspend_count_lock_);

  static MemberOffset MonitorOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Object, monitor_);
  }

  // As_volatile can be false if the mutators are suspended. This is an optimization since it
  // avoids the barriers.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  LockWord GetLockWord(bool as_volatile) REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetLockWord(LockWord new_val, bool as_volatile) REQUIRES_SHARED(Locks::mutator_lock_);
  bool CasLockWordWeakSequentiallyConsistent(LockWord old_val, LockWord new_val)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool CasLockWordWeakRelaxed(LockWord old_val, LockWord new_val)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool CasLockWordWeakAcquire(LockWord old_val, LockWord new_val)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool CasLockWordWeakRelease(LockWord old_val, LockWord new_val)
      REQUIRES_SHARED(Locks::mutator_lock_);
  uint32_t GetLockOwnerThreadId();

  // Try to enter the monitor, returns non null if we succeeded.
  mirror::Object* MonitorTryEnter(Thread* self)
      EXCLUSIVE_LOCK_FUNCTION()
      REQUIRES(!Roles::uninterruptible_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* MonitorEnter(Thread* self)
      EXCLUSIVE_LOCK_FUNCTION()
      REQUIRES(!Roles::uninterruptible_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool MonitorExit(Thread* self)
      REQUIRES(!Roles::uninterruptible_)
      REQUIRES_SHARED(Locks::mutator_lock_)
      UNLOCK_FUNCTION();
  void Notify(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void NotifyAll(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void Wait(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void Wait(Thread* self, int64_t timeout, int32_t nanos) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsClass() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  Class* AsClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsObjectArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<class T,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjectArray<T>* AsObjectArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ClassLoader* AsClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsDexCache() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  DexCache* AsDexCache() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsArrayInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  Array* AsArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  BooleanArray* AsBooleanArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ByteArray* AsByteArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ByteArray* AsByteSizedArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  CharArray* AsCharArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ShortArray* AsShortArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ShortArray* AsShortSizedArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsIntArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  IntArray* AsIntArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsLongArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  LongArray* AsLongArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsFloatArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  FloatArray* AsFloatArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsDoubleArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  DoubleArray* AsDoubleArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsString() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  String* AsString() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  Throwable* AsThrowable() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  Reference* AsReference() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsWeakReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsSoftReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsFinalizerReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  FinalizerReference* AsFinalizerReference() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPhantomReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);

  // Accessor for Java type fields.
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
      ReadBarrierOption kReadBarrierOption = kWithReadBarrier, bool kIsVolatile = false>
  ALWAYS_INLINE T* GetFieldObject(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
      ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE T* GetFieldObjectVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                       ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldObject(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldObjectVolatile(MemberOffset field_offset,
                                            ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakSequentiallyConsistentObject(MemberOffset field_offset,
                                                ObjPtr<Object> old_value,
                                                ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakSequentiallyConsistentObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                                   ObjPtr<Object> old_value,
                                                                   ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongSequentiallyConsistentObject(MemberOffset field_offset,
                                                  ObjPtr<Object> old_value,
                                                  ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongSequentiallyConsistentObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                                     ObjPtr<Object> old_value,
                                                                     ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakRelaxedObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                    ObjPtr<Object> old_value,
                                                    ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakReleaseObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                    ObjPtr<Object> old_value,
                                                    ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongRelaxedObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                      ObjPtr<Object> old_value,
                                                      ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongReleaseObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                      ObjPtr<Object> old_value,
                                                      ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  HeapReference<Object>* GetFieldObjectReferenceAddr(MemberOffset field_offset);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE uint8_t GetFieldBoolean(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int8_t GetFieldByte(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint8_t GetFieldBooleanVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int8_t GetFieldByteVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldBoolean(MemberOffset field_offset, uint8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldByte(MemberOffset field_offset, int8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldBooleanVolatile(MemberOffset field_offset, uint8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldByteVolatile(MemberOffset field_offset, int8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE uint16_t GetFieldChar(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int16_t GetFieldShort(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint16_t GetFieldCharVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int16_t GetFieldShortVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldChar(MemberOffset field_offset, uint16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldShort(MemberOffset field_offset, int16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldCharVolatile(MemberOffset field_offset, uint16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldShortVolatile(MemberOffset field_offset, int16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int32_t GetField32(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kVerifyFlags & kVerifyThis) {
      VerifyObject(this);
    }
    return GetField<int32_t, kIsVolatile>(field_offset);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int32_t GetField32Volatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags, true>(field_offset);
  }

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE void SetField32(MemberOffset field_offset, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetField32Volatile(MemberOffset field_offset, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool CasFieldWeakSequentiallyConsistent32(MemberOffset field_offset,
                                                          int32_t old_value, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakRelaxed32(MemberOffset field_offset, int32_t old_value,
                             int32_t new_value) ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakAcquire32(MemberOffset field_offset, int32_t old_value,
                             int32_t new_value) ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakRelease32(MemberOffset field_offset, int32_t old_value,
                             int32_t new_value) ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongSequentiallyConsistent32(MemberOffset field_offset, int32_t old_value,
                                              int32_t new_value) ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int64_t GetField64(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kVerifyFlags & kVerifyThis) {
      VerifyObject(this);
    }
    return GetField<int64_t, kIsVolatile>(field_offset);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int64_t GetField64Volatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField64<kVerifyFlags, true>(field_offset);
  }

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE void SetField64(MemberOffset field_offset, int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetField64Volatile(MemberOffset field_offset, int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakSequentiallyConsistent64(MemberOffset field_offset, int64_t old_value,
                                            int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongSequentiallyConsistent64(MemberOffset field_offset, int64_t old_value,
                                              int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, typename T>
  void SetFieldPtr(MemberOffset field_offset, T new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SetFieldPtrWithSize<kTransactionActive, kCheckTransaction, kVerifyFlags>(
        field_offset, new_value, kRuntimePointerSize);
  }
  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, typename T>
  void SetFieldPtr64(MemberOffset field_offset, T new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SetFieldPtrWithSize<kTransactionActive, kCheckTransaction, kVerifyFlags>(
        field_offset, new_value, 8u);
  }

  template<bool kTransactionActive, bool kCheckTransaction = true,
      VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, typename T>
  ALWAYS_INLINE void SetFieldPtrWithSize(MemberOffset field_offset,
                                         T new_value,
                                         PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (pointer_size == PointerSize::k32) {
      uintptr_t ptr  = reinterpret_cast<uintptr_t>(new_value);
      DCHECK_EQ(static_cast<uint32_t>(ptr), ptr);  // Check that we dont lose any non 0 bits.
      SetField32<kTransactionActive, kCheckTransaction, kVerifyFlags>(
          field_offset, static_cast<int32_t>(static_cast<uint32_t>(ptr)));
    } else {
      SetField64<kTransactionActive, kCheckTransaction, kVerifyFlags>(
          field_offset, reinterpret_cast64<int64_t>(new_value));
    }
  }
  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template <bool kVisitNativeRoots = true,
            VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor,
            typename JavaLangRefVisitor = VoidFunctor>
  void VisitReferences(const Visitor& visitor, const JavaLangRefVisitor& ref_visitor)
      NO_THREAD_SAFETY_ANALYSIS;

  ArtField* FindFieldByOffset(MemberOffset offset) REQUIRES_SHARED(Locks::mutator_lock_);

  // Used by object_test.
  static void SetHashCodeSeed(uint32_t new_seed);
  // Generate an identity hash code. Public for object test.
  static uint32_t GenerateIdentityHashCode();

  // Returns a human-readable form of the name of the *class* of the given object.
  // So given an instance of java.lang.String, the output would
  // be "java.lang.String". Given an array of int, the output would be "int[]".
  // Given String.class, the output would be "java.lang.Class<java.lang.String>".
  static std::string PrettyTypeOf(ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyTypeOf()
      REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  // Accessors for non-Java type fields
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  T GetFieldPtr(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldPtrWithSize<T, kVerifyFlags, kIsVolatile>(field_offset, kRuntimePointerSize);
  }
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  T GetFieldPtr64(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldPtrWithSize<T, kVerifyFlags, kIsVolatile>(field_offset,
                                                             PointerSize::k64);
  }

  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE T GetFieldPtrWithSize(MemberOffset field_offset, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (pointer_size == PointerSize::k32) {
      uint64_t address = static_cast<uint32_t>(GetField32<kVerifyFlags, kIsVolatile>(field_offset));
      return reinterpret_cast<T>(static_cast<uintptr_t>(address));
    } else {
      int64_t v = GetField64<kVerifyFlags, kIsVolatile>(field_offset);
      return reinterpret_cast64<T>(v);
    }
  }

  // TODO: Fixme when anotatalysis works with visitors.
  template<bool kIsStatic,
          VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
          ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
          typename Visitor>
  void VisitFieldsReferences(uint32_t ref_offsets, const Visitor& visitor) HOT_ATTR
      NO_THREAD_SAFETY_ANALYSIS;
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
           typename Visitor>
  void VisitInstanceFieldsReferences(ObjPtr<mirror::Class> klass, const Visitor& visitor) HOT_ATTR
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
           typename Visitor>
  void VisitStaticFieldsReferences(ObjPtr<mirror::Class> klass, const Visitor& visitor) HOT_ATTR
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  template<typename kSize, bool kIsVolatile>
  ALWAYS_INLINE void SetField(MemberOffset field_offset, kSize new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
    kSize* addr = reinterpret_cast<kSize*>(raw_addr);
    if (kIsVolatile) {
      reinterpret_cast<Atomic<kSize>*>(addr)->StoreSequentiallyConsistent(new_value);
    } else {
      reinterpret_cast<Atomic<kSize>*>(addr)->StoreJavaData(new_value);
    }
  }

  template<typename kSize, bool kIsVolatile>
  ALWAYS_INLINE kSize GetField(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const uint8_t* raw_addr = reinterpret_cast<const uint8_t*>(this) + field_offset.Int32Value();
    const kSize* addr = reinterpret_cast<const kSize*>(raw_addr);
    if (kIsVolatile) {
      return reinterpret_cast<const Atomic<kSize>*>(addr)->LoadSequentiallyConsistent();
    } else {
      return reinterpret_cast<const Atomic<kSize>*>(addr)->LoadJavaData();
    }
  }

  // Get a field with acquire semantics.
  template<typename kSize>
  ALWAYS_INLINE kSize GetFieldAcquire(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Verify the type correctness of stores to fields.
  // TODO: This can cause thread suspension and isn't moving GC safe.
  void CheckFieldAssignmentImpl(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CheckFieldAssignment(MemberOffset field_offset, ObjPtr<Object>new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kCheckFieldAssignments) {
      CheckFieldAssignmentImpl(field_offset, new_value);
    }
  }

  // A utility function that copies an object in a read barrier and write barrier-aware way.
  // This is internally used by Clone() and Class::CopyOf(). If the object is finalizable,
  // it is the callers job to call Heap::AddFinalizerReference.
  static Object* CopyObject(ObjPtr<mirror::Object> dest,
                            ObjPtr<mirror::Object> src,
                            size_t num_bytes)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static Atomic<uint32_t> hash_code_seed;

  // The Class representing the type of the object.
  HeapReference<Class> klass_;
  // Monitor and hash code information.
  uint32_t monitor_;

#ifdef USE_BROOKS_READ_BARRIER
  // Note names use a 'x' prefix and the x_rb_ptr_ is of type int
  // instead of Object to go with the alphabetical/by-type field order
  // on the Java side.
  uint32_t x_rb_ptr_;      // For the Brooks pointer.
  uint32_t x_xpadding_;    // For 8-byte alignment. TODO: get rid of this.
#endif

  friend class art::ImageWriter;
  friend class art::Monitor;
  friend struct art::ObjectOffsets;  // for verifying offset information
  friend class CopyObjectVisitor;  // for CopyObject().
  friend class CopyClassVisitor;   // for CopyObject().
  DISALLOW_ALLOCATION();
  DISALLOW_IMPLICIT_CONSTRUCTORS(Object);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_H_
