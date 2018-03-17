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

#ifndef ART_RUNTIME_MIRROR_OBJECT_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_INL_H_

#include "object.h"

#include "art_field.h"
#include "art_method.h"
#include "atomic.h"
#include "array-inl.h"
#include "class.h"
#include "class_flags.h"
#include "class_linker.h"
#include "class_loader-inl.h"
#include "dex_cache-inl.h"
#include "lock_word-inl.h"
#include "monitor.h"
#include "object_array-inl.h"
#include "read_barrier-inl.h"
#include "reference.h"
#include "runtime.h"
#include "string-inl.h"
#include "throwable.h"

namespace art {
namespace mirror {

inline uint32_t Object::ClassSize(size_t pointer_size) {
  uint32_t vtable_entries = kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Class* Object::GetClass() {
  return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Object, klass_));
}

template<VerifyObjectFlags kVerifyFlags>
inline void Object::SetClass(Class* new_klass) {
  // new_klass may be null prior to class linker initialization.
  // We don't mark the card as this occurs as part of object allocation. Not all objects have
  // backing cards, such as large objects.
  // We use non transactional version since we can't undo this write. We also disable checking as
  // we may run in transaction mode here.
  SetFieldObjectWithoutWriteBarrier<false, false,
      static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis)>(
      OFFSET_OF_OBJECT_MEMBER(Object, klass_), new_klass);
}

template<VerifyObjectFlags kVerifyFlags>
inline LockWord Object::GetLockWord(bool as_volatile) {
  if (as_volatile) {
    return LockWord(GetField32Volatile<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Object, monitor_)));
  }
  return LockWord(GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Object, monitor_)));
}

template<VerifyObjectFlags kVerifyFlags>
inline void Object::SetLockWord(LockWord new_val, bool as_volatile) {
  // Force use of non-transactional mode and do not check.
  if (as_volatile) {
    SetField32Volatile<false, false, kVerifyFlags>(
        OFFSET_OF_OBJECT_MEMBER(Object, monitor_), new_val.GetValue());
  } else {
    SetField32<false, false, kVerifyFlags>(
        OFFSET_OF_OBJECT_MEMBER(Object, monitor_), new_val.GetValue());
  }
}

inline bool Object::CasLockWordWeakSequentiallyConsistent(LockWord old_val, LockWord new_val) {
  // Force use of non-transactional mode and do not check.
  return CasFieldWeakSequentiallyConsistent32<false, false>(
      OFFSET_OF_OBJECT_MEMBER(Object, monitor_), old_val.GetValue(), new_val.GetValue());
}

inline bool Object::CasLockWordWeakRelaxed(LockWord old_val, LockWord new_val) {
  // Force use of non-transactional mode and do not check.
  return CasFieldWeakRelaxed32<false, false>(
      OFFSET_OF_OBJECT_MEMBER(Object, monitor_), old_val.GetValue(), new_val.GetValue());
}

inline bool Object::CasLockWordWeakRelease(LockWord old_val, LockWord new_val) {
  // Force use of non-transactional mode and do not check.
  return CasFieldWeakRelease32<false, false>(
      OFFSET_OF_OBJECT_MEMBER(Object, monitor_), old_val.GetValue(), new_val.GetValue());
}

inline uint32_t Object::GetLockOwnerThreadId() {
  return Monitor::GetLockOwnerThreadId(this);
}

inline mirror::Object* Object::MonitorEnter(Thread* self) {
  return Monitor::MonitorEnter(self, this, /*trylock*/false);
}

inline mirror::Object* Object::MonitorTryEnter(Thread* self) {
  return Monitor::MonitorEnter(self, this, /*trylock*/true);
}

inline bool Object::MonitorExit(Thread* self) {
  return Monitor::MonitorExit(self, this);
}

inline void Object::Notify(Thread* self) {
  Monitor::Notify(self, this);
}

inline void Object::NotifyAll(Thread* self) {
  Monitor::NotifyAll(self, this);
}

inline void Object::Wait(Thread* self) {
  Monitor::Wait(self, this, 0, 0, true, kWaiting);
}

inline void Object::Wait(Thread* self, int64_t ms, int32_t ns) {
  Monitor::Wait(self, this, ms, ns, true, kTimedWaiting);
}

inline Object* Object::GetReadBarrierPointer() {
#ifdef USE_BAKER_READ_BARRIER
  DCHECK(kUseBakerReadBarrier);
  return reinterpret_cast<Object*>(GetLockWord(false).ReadBarrierState());
#elif USE_BROOKS_READ_BARRIER
  DCHECK(kUseBrooksReadBarrier);
  return GetFieldObject<Object, kVerifyNone, kWithoutReadBarrier>(
      OFFSET_OF_OBJECT_MEMBER(Object, x_rb_ptr_));
#else
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
#endif
}

inline void Object::SetReadBarrierPointer(Object* rb_ptr) {
#ifdef USE_BAKER_READ_BARRIER
  DCHECK(kUseBakerReadBarrier);
  DCHECK_EQ(reinterpret_cast<uint64_t>(rb_ptr) >> 32, 0U);
  LockWord lw = GetLockWord(false);
  lw.SetReadBarrierState(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rb_ptr)));
  SetLockWord(lw, false);
#elif USE_BROOKS_READ_BARRIER
  DCHECK(kUseBrooksReadBarrier);
  // We don't mark the card as this occurs as part of object allocation. Not all objects have
  // backing cards, such as large objects.
  SetFieldObjectWithoutWriteBarrier<false, false, kVerifyNone>(
      OFFSET_OF_OBJECT_MEMBER(Object, x_rb_ptr_), rb_ptr);
#else
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
  UNUSED(rb_ptr);
#endif
}

template<bool kCasRelease>
inline bool Object::AtomicSetReadBarrierPointer(Object* expected_rb_ptr, Object* rb_ptr) {
#ifdef USE_BAKER_READ_BARRIER
  DCHECK(kUseBakerReadBarrier);
  DCHECK_EQ(reinterpret_cast<uint64_t>(expected_rb_ptr) >> 32, 0U);
  DCHECK_EQ(reinterpret_cast<uint64_t>(rb_ptr) >> 32, 0U);
  LockWord expected_lw;
  LockWord new_lw;
  do {
    LockWord lw = GetLockWord(false);
    if (UNLIKELY(reinterpret_cast<Object*>(lw.ReadBarrierState()) != expected_rb_ptr)) {
      // Lost the race.
      return false;
    }
    expected_lw = lw;
    expected_lw.SetReadBarrierState(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(expected_rb_ptr)));
    new_lw = lw;
    new_lw.SetReadBarrierState(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rb_ptr)));
    // ConcurrentCopying::ProcessMarkStackRef uses this with kCasRelease == true.
    // If kCasRelease == true, use a CAS release so that when GC updates all the fields of
    // an object and then changes the object from gray to black, the field updates (stores) will be
    // visible (won't be reordered after this CAS.)
  } while (!(kCasRelease ?
             CasLockWordWeakRelease(expected_lw, new_lw) :
             CasLockWordWeakRelaxed(expected_lw, new_lw)));
  return true;
#elif USE_BROOKS_READ_BARRIER
  DCHECK(kUseBrooksReadBarrier);
  MemberOffset offset = OFFSET_OF_OBJECT_MEMBER(Object, x_rb_ptr_);
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + offset.SizeValue();
  Atomic<uint32_t>* atomic_rb_ptr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);
  HeapReference<Object> expected_ref(HeapReference<Object>::FromMirrorPtr(expected_rb_ptr));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(rb_ptr));
  do {
    if (UNLIKELY(atomic_rb_ptr->LoadRelaxed() != expected_ref.reference_)) {
      // Lost the race.
      return false;
    }
  } while (!atomic_rb_ptr->CompareExchangeWeakSequentiallyConsistent(expected_ref.reference_,
                                                                     new_ref.reference_));
  return true;
#else
  UNUSED(expected_rb_ptr, rb_ptr);
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
#endif
}

inline void Object::AssertReadBarrierPointer() const {
  if (kUseBakerReadBarrier) {
    Object* obj = const_cast<Object*>(this);
    DCHECK(obj->GetReadBarrierPointer() == nullptr)
        << "Bad Baker pointer: obj=" << reinterpret_cast<void*>(obj)
        << " ptr=" << reinterpret_cast<void*>(obj->GetReadBarrierPointer());
  } else {
    CHECK(kUseBrooksReadBarrier);
    Object* obj = const_cast<Object*>(this);
    DCHECK_EQ(obj, obj->GetReadBarrierPointer())
        << "Bad Brooks pointer: obj=" << reinterpret_cast<void*>(obj)
        << " ptr=" << reinterpret_cast<void*>(obj->GetReadBarrierPointer());
  }
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::VerifierInstanceOf(Class* klass) {
  DCHECK(klass != nullptr);
  DCHECK(GetClass<kVerifyFlags>() != nullptr);
  return klass->IsInterface() || InstanceOf(klass);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::InstanceOf(Class* klass) {
  DCHECK(klass != nullptr);
  DCHECK(GetClass<kVerifyNone>() != nullptr);
  return klass->IsAssignableFrom(GetClass<kVerifyFlags>());
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsClass() {
  Class* java_lang_Class = GetClass<kVerifyFlags, kReadBarrierOption>()->
      template GetClass<kVerifyFlags, kReadBarrierOption>();
  return GetClass<static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis),
      kReadBarrierOption>() == java_lang_Class;
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Class* Object::AsClass() {
  DCHECK((IsClass<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<Class*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsObjectArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  return IsArrayInstance<kVerifyFlags, kReadBarrierOption>() &&
      !GetClass<kNewFlags, kReadBarrierOption>()->
          template GetComponentType<kNewFlags, kReadBarrierOption>()->IsPrimitive();
}

template<class T, VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjectArray<T>* Object::AsObjectArray() {
  DCHECK((IsObjectArray<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<ObjectArray<T>*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsArrayInstance() {
  return GetClass<kVerifyFlags, kReadBarrierOption>()->
      template IsArrayClass<kVerifyFlags, kReadBarrierOption>();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsReferenceInstance() {
  return GetClass<kVerifyFlags, kReadBarrierOption>()->IsTypeOfReferenceClass();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Reference* Object::AsReference() {
  DCHECK((IsReferenceInstance<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<Reference*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Array* Object::AsArray() {
  DCHECK((IsArrayInstance<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<Array*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline BooleanArray* Object::AsBooleanArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->GetComponentType()->IsPrimitiveBoolean());
  return down_cast<BooleanArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline ByteArray* Object::AsByteArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveByte());
  return down_cast<ByteArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline ByteArray* Object::AsByteSizedArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveByte() ||
         GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveBoolean());
  return down_cast<ByteArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline CharArray* Object::AsCharArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveChar());
  return down_cast<CharArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline ShortArray* Object::AsShortArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveShort());
  return down_cast<ShortArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline ShortArray* Object::AsShortSizedArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveShort() ||
         GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveChar());
  return down_cast<ShortArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsIntArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  mirror::Class* klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  mirror::Class* component_type = klass->GetComponentType<kVerifyFlags, kReadBarrierOption>();
  return component_type != nullptr && component_type->template IsPrimitiveInt<kNewFlags>();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline IntArray* Object::AsIntArray() {
  DCHECK((IsIntArray<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<IntArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsLongArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  mirror::Class* klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  mirror::Class* component_type = klass->GetComponentType<kVerifyFlags, kReadBarrierOption>();
  return component_type != nullptr && component_type->template IsPrimitiveLong<kNewFlags>();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline LongArray* Object::AsLongArray() {
  DCHECK((IsLongArray<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<LongArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::IsFloatArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  auto* component_type = GetClass<kVerifyFlags>()->GetComponentType();
  return component_type != nullptr && component_type->template IsPrimitiveFloat<kNewFlags>();
}

template<VerifyObjectFlags kVerifyFlags>
inline FloatArray* Object::AsFloatArray() {
  DCHECK(IsFloatArray<kVerifyFlags>());
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveFloat());
  return down_cast<FloatArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::IsDoubleArray() {
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  auto* component_type = GetClass<kVerifyFlags>()->GetComponentType();
  return component_type != nullptr && component_type->template IsPrimitiveDouble<kNewFlags>();
}

template<VerifyObjectFlags kVerifyFlags>
inline DoubleArray* Object::AsDoubleArray() {
  DCHECK(IsDoubleArray<kVerifyFlags>());
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  DCHECK(GetClass<kVerifyFlags>()->IsArrayClass());
  DCHECK(GetClass<kNewFlags>()->template GetComponentType<kNewFlags>()->IsPrimitiveDouble());
  return down_cast<DoubleArray*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsString() {
  return GetClass<kVerifyFlags, kReadBarrierOption>()->IsStringClass();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline String* Object::AsString() {
  DCHECK((IsString<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<String*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline Throwable* Object::AsThrowable() {
  DCHECK(GetClass<kVerifyFlags>()->IsThrowableClass());
  return down_cast<Throwable*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::IsWeakReferenceInstance() {
  return GetClass<kVerifyFlags>()->IsWeakReferenceClass();
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::IsSoftReferenceInstance() {
  return GetClass<kVerifyFlags>()->IsSoftReferenceClass();
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::IsFinalizerReferenceInstance() {
  return GetClass<kVerifyFlags>()->IsFinalizerReferenceClass();
}

template<VerifyObjectFlags kVerifyFlags>
inline FinalizerReference* Object::AsFinalizerReference() {
  DCHECK(IsFinalizerReferenceInstance<kVerifyFlags>());
  return down_cast<FinalizerReference*>(this);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::IsPhantomReferenceInstance() {
  return GetClass<kVerifyFlags>()->IsPhantomReferenceClass();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline size_t Object::SizeOf() {
  size_t result;
  constexpr auto kNewFlags = static_cast<VerifyObjectFlags>(kVerifyFlags & ~kVerifyThis);
  if (IsArrayInstance<kVerifyFlags, kReadBarrierOption>()) {
    result = AsArray<kNewFlags, kReadBarrierOption>()->
        template SizeOf<kNewFlags, kReadBarrierOption>();
  } else if (IsClass<kNewFlags, kReadBarrierOption>()) {
    result = AsClass<kNewFlags, kReadBarrierOption>()->
        template SizeOf<kNewFlags, kReadBarrierOption>();
  } else if (GetClass<kNewFlags, kReadBarrierOption>()->IsStringClass()) {
    result = AsString<kNewFlags, kReadBarrierOption>()->
        template SizeOf<kNewFlags>();
  } else {
    result = GetClass<kNewFlags, kReadBarrierOption>()->
        template GetObjectSize<kNewFlags, kReadBarrierOption>();
  }
  DCHECK_GE(result, sizeof(Object))
      << " class=" << PrettyTypeOf(GetClass<kNewFlags, kReadBarrierOption>());
  return result;
}

template<VerifyObjectFlags kVerifyFlags, bool kIsVolatile>
inline uint8_t Object::GetFieldBoolean(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return GetField<uint8_t, kIsVolatile>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags, bool kIsVolatile>
inline int8_t Object::GetFieldByte(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return GetField<int8_t, kIsVolatile>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline uint8_t Object::GetFieldBooleanVolatile(MemberOffset field_offset) {
  return GetFieldBoolean<kVerifyFlags, true>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline int8_t Object::GetFieldByteVolatile(MemberOffset field_offset) {
  return GetFieldByte<kVerifyFlags, true>(field_offset);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldBoolean(MemberOffset field_offset, uint8_t new_value)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldBoolean(this, field_offset,
                                           GetFieldBoolean<kVerifyFlags, kIsVolatile>(field_offset),
                                           kIsVolatile);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  SetField<uint8_t, kIsVolatile>(field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldByte(MemberOffset field_offset, int8_t new_value)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldByte(this, field_offset,
                                           GetFieldByte<kVerifyFlags, kIsVolatile>(field_offset),
                                           kIsVolatile);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  SetField<int8_t, kIsVolatile>(field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetFieldBooleanVolatile(MemberOffset field_offset, uint8_t new_value) {
  return SetFieldBoolean<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(
      field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetFieldByteVolatile(MemberOffset field_offset, int8_t new_value) {
  return SetFieldByte<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(
      field_offset, new_value);
}

template<VerifyObjectFlags kVerifyFlags, bool kIsVolatile>
inline uint16_t Object::GetFieldChar(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return GetField<uint16_t, kIsVolatile>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags, bool kIsVolatile>
inline int16_t Object::GetFieldShort(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return GetField<int16_t, kIsVolatile>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline uint16_t Object::GetFieldCharVolatile(MemberOffset field_offset) {
  return GetFieldChar<kVerifyFlags, true>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline int16_t Object::GetFieldShortVolatile(MemberOffset field_offset) {
  return GetFieldShort<kVerifyFlags, true>(field_offset);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldChar(MemberOffset field_offset, uint16_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldChar(this, field_offset,
                                           GetFieldChar<kVerifyFlags, kIsVolatile>(field_offset),
                                           kIsVolatile);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  SetField<uint16_t, kIsVolatile>(field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldShort(MemberOffset field_offset, int16_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldChar(this, field_offset,
                                           GetFieldShort<kVerifyFlags, kIsVolatile>(field_offset),
                                           kIsVolatile);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  SetField<int16_t, kIsVolatile>(field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetFieldCharVolatile(MemberOffset field_offset, uint16_t new_value) {
  return SetFieldChar<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(
      field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetFieldShortVolatile(MemberOffset field_offset, int16_t new_value) {
  return SetFieldShort<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(
      field_offset, new_value);
}

template<VerifyObjectFlags kVerifyFlags, bool kIsVolatile>
inline int32_t Object::GetField32(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return GetField<int32_t, kIsVolatile>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline int32_t Object::GetField32Volatile(MemberOffset field_offset) {
  return GetField32<kVerifyFlags, true>(field_offset);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetField32(MemberOffset field_offset, int32_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset,
                                           GetField32<kVerifyFlags, kIsVolatile>(field_offset),
                                           kIsVolatile);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  SetField<int32_t, kIsVolatile>(field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetField32Volatile(MemberOffset field_offset, int32_t new_value) {
  SetField32<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(field_offset, new_value);
}

// TODO: Pass memory_order_ and strong/weak as arguments to avoid code duplication?

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakSequentiallyConsistent32(MemberOffset field_offset,
                                                         int32_t old_value, int32_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  AtomicInteger* atomic_addr = reinterpret_cast<AtomicInteger*>(raw_addr);

  return atomic_addr->CompareExchangeWeakSequentiallyConsistent(old_value, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakRelaxed32(MemberOffset field_offset,
                                          int32_t old_value, int32_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  AtomicInteger* atomic_addr = reinterpret_cast<AtomicInteger*>(raw_addr);

  return atomic_addr->CompareExchangeWeakRelaxed(old_value, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakRelease32(MemberOffset field_offset,
                                          int32_t old_value, int32_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  AtomicInteger* atomic_addr = reinterpret_cast<AtomicInteger*>(raw_addr);

  return atomic_addr->CompareExchangeWeakRelease(old_value, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongSequentiallyConsistent32(MemberOffset field_offset,
                                                           int32_t old_value, int32_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  AtomicInteger* atomic_addr = reinterpret_cast<AtomicInteger*>(raw_addr);

  return atomic_addr->CompareExchangeStrongSequentiallyConsistent(old_value, new_value);
}

template<VerifyObjectFlags kVerifyFlags, bool kIsVolatile>
inline int64_t Object::GetField64(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return GetField<int64_t, kIsVolatile>(field_offset);
}

template<VerifyObjectFlags kVerifyFlags>
inline int64_t Object::GetField64Volatile(MemberOffset field_offset) {
  return GetField64<kVerifyFlags, true>(field_offset);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetField64(MemberOffset field_offset, int64_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField64(this, field_offset,
                                           GetField64<kVerifyFlags, kIsVolatile>(field_offset),
                                           kIsVolatile);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  SetField<int64_t, kIsVolatile>(field_offset, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetField64Volatile(MemberOffset field_offset, int64_t new_value) {
  return SetField64<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(field_offset,
                                                                               new_value);
}

template<typename kSize, bool kIsVolatile>
inline void Object::SetField(MemberOffset field_offset, kSize new_value) {
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  kSize* addr = reinterpret_cast<kSize*>(raw_addr);
  if (kIsVolatile) {
    reinterpret_cast<Atomic<kSize>*>(addr)->StoreSequentiallyConsistent(new_value);
  } else {
    reinterpret_cast<Atomic<kSize>*>(addr)->StoreJavaData(new_value);
  }
}

template<typename kSize, bool kIsVolatile>
inline kSize Object::GetField(MemberOffset field_offset) {
  const uint8_t* raw_addr = reinterpret_cast<const uint8_t*>(this) + field_offset.Int32Value();
  const kSize* addr = reinterpret_cast<const kSize*>(raw_addr);
  if (kIsVolatile) {
    return reinterpret_cast<const Atomic<kSize>*>(addr)->LoadSequentiallyConsistent();
  } else {
    return reinterpret_cast<const Atomic<kSize>*>(addr)->LoadJavaData();
  }
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakSequentiallyConsistent64(MemberOffset field_offset,
                                                         int64_t old_value, int64_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField64(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<int64_t>* atomic_addr = reinterpret_cast<Atomic<int64_t>*>(raw_addr);
  return atomic_addr->CompareExchangeWeakSequentiallyConsistent(old_value, new_value);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongSequentiallyConsistent64(MemberOffset field_offset,
                                                           int64_t old_value, int64_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField64(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<int64_t>* atomic_addr = reinterpret_cast<Atomic<int64_t>*>(raw_addr);
  return atomic_addr->CompareExchangeStrongSequentiallyConsistent(old_value, new_value);
}

template<class T, VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption,
         bool kIsVolatile>
inline T* Object::GetFieldObject(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  HeapReference<T>* objref_addr = reinterpret_cast<HeapReference<T>*>(raw_addr);
  T* result = ReadBarrier::Barrier<T, kReadBarrierOption>(this, field_offset, objref_addr);
  if (kIsVolatile) {
    // TODO: Refactor to use a SequentiallyConsistent load instead.
    QuasiAtomic::ThreadFenceAcquire();  // Ensure visibility of operations preceding store.
  }
  if (kVerifyFlags & kVerifyReads) {
    VerifyObject(result);
  }
  return result;
}

template<class T, VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline T* Object::GetFieldObjectVolatile(MemberOffset field_offset) {
  return GetFieldObject<T, kVerifyFlags, kReadBarrierOption, true>(field_offset);
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                      Object* new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    mirror::Object* obj;
    if (kIsVolatile) {
      obj = GetFieldObjectVolatile<Object>(field_offset);
    } else {
      obj = GetFieldObject<Object>(field_offset);
    }
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, obj, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  if (kVerifyFlags & kVerifyWrites) {
    VerifyObject(new_value);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  HeapReference<Object>* objref_addr = reinterpret_cast<HeapReference<Object>*>(raw_addr);
  if (kIsVolatile) {
    // TODO: Refactor to use a SequentiallyConsistent store instead.
    QuasiAtomic::ThreadFenceRelease();  // Ensure that prior accesses are visible before store.
    objref_addr->Assign(new_value);
    QuasiAtomic::ThreadFenceSequentiallyConsistent();
                                // Ensure this store occurs before any volatile loads.
  } else {
    objref_addr->Assign(new_value);
  }
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldObject(MemberOffset field_offset, Object* new_value) {
  SetFieldObjectWithoutWriteBarrier<kTransactionActive, kCheckTransaction, kVerifyFlags,
      kIsVolatile>(field_offset, new_value);
  if (new_value != nullptr) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
    // TODO: Check field assignment could theoretically cause thread suspension, TODO: fix this.
    CheckFieldAssignment(field_offset, new_value);
  }
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetFieldObjectVolatile(MemberOffset field_offset, Object* new_value) {
  SetFieldObject<kTransactionActive, kCheckTransaction, kVerifyFlags, true>(field_offset,
                                                                            new_value);
}

template <VerifyObjectFlags kVerifyFlags>
inline HeapReference<Object>* Object::GetFieldObjectReferenceAddr(MemberOffset field_offset) {
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  return reinterpret_cast<HeapReference<Object>*>(reinterpret_cast<uint8_t*>(this) +
      field_offset.Int32Value());
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakSequentiallyConsistentObject(MemberOffset field_offset,
                                                             Object* old_value, Object* new_value) {
  bool success = CasFieldWeakSequentiallyConsistentObjectWithoutWriteBarrier<
      kTransactionActive, kCheckTransaction, kVerifyFlags>(field_offset, old_value, new_value);
  if (success) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
  }
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakSequentiallyConsistentObjectWithoutWriteBarrier(
    MemberOffset field_offset, Object* old_value, Object* new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  if (kVerifyFlags & kVerifyWrites) {
    VerifyObject(new_value);
  }
  if (kVerifyFlags & kVerifyReads) {
    VerifyObject(old_value);
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, old_value, true);
  }
  HeapReference<Object> old_ref(HeapReference<Object>::FromMirrorPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeWeakSequentiallyConsistent(old_ref.reference_,
                                                                        new_ref.reference_);
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongSequentiallyConsistentObject(MemberOffset field_offset,
                                                               Object* old_value, Object* new_value) {
  bool success = CasFieldStrongSequentiallyConsistentObjectWithoutWriteBarrier<
      kTransactionActive, kCheckTransaction, kVerifyFlags>(field_offset, old_value, new_value);
  if (success) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
  }
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongSequentiallyConsistentObjectWithoutWriteBarrier(
    MemberOffset field_offset, Object* old_value, Object* new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  if (kVerifyFlags & kVerifyWrites) {
    VerifyObject(new_value);
  }
  if (kVerifyFlags & kVerifyReads) {
    VerifyObject(old_value);
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, old_value, true);
  }
  HeapReference<Object> old_ref(HeapReference<Object>::FromMirrorPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeStrongSequentiallyConsistent(old_ref.reference_,
                                                                          new_ref.reference_);
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakRelaxedObjectWithoutWriteBarrier(
    MemberOffset field_offset, Object* old_value, Object* new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  if (kVerifyFlags & kVerifyWrites) {
    VerifyObject(new_value);
  }
  if (kVerifyFlags & kVerifyReads) {
    VerifyObject(old_value);
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, old_value, true);
  }
  HeapReference<Object> old_ref(HeapReference<Object>::FromMirrorPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeWeakRelaxed(old_ref.reference_,
                                                         new_ref.reference_);
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongRelaxedObjectWithoutWriteBarrier(
    MemberOffset field_offset, Object* old_value, Object* new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  if (kVerifyFlags & kVerifyWrites) {
    VerifyObject(new_value);
  }
  if (kVerifyFlags & kVerifyReads) {
    VerifyObject(old_value);
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, old_value, true);
  }
  HeapReference<Object> old_ref(HeapReference<Object>::FromMirrorPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeStrongRelaxed(old_ref.reference_,
                                                           new_ref.reference_);
  return success;
}

template<bool kIsStatic,
         VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption,
         typename Visitor>
inline void Object::VisitFieldsReferences(uint32_t ref_offsets, const Visitor& visitor) {
  if (!kIsStatic && (ref_offsets != mirror::Class::kClassWalkSuper)) {
    // Instance fields and not the slow-path.
    uint32_t field_offset = mirror::kObjectHeaderSize;
    while (ref_offsets != 0) {
      if ((ref_offsets & 1) != 0) {
        visitor(this, MemberOffset(field_offset), kIsStatic);
      }
      ref_offsets >>= 1;
      field_offset += sizeof(mirror::HeapReference<mirror::Object>);
    }
  } else {
    // There is no reference offset bitmap. In the non-static case, walk up the class
    // inheritance hierarchy and find reference offsets the hard way. In the static case, just
    // consider this class.
    for (mirror::Class* klass = kIsStatic
            ? AsClass<kVerifyFlags, kReadBarrierOption>()
            : GetClass<kVerifyFlags, kReadBarrierOption>();
        klass != nullptr;
        klass = kIsStatic ? nullptr : klass->GetSuperClass<kVerifyFlags, kReadBarrierOption>()) {
      const size_t num_reference_fields =
          kIsStatic ? klass->NumReferenceStaticFields() : klass->NumReferenceInstanceFields();
      if (num_reference_fields == 0u) {
        continue;
      }
      // Presumably GC can happen when we are cross compiling, it should not cause performance
      // problems to do pointer size logic.
      MemberOffset field_offset = kIsStatic
          ? klass->GetFirstReferenceStaticFieldOffset<kVerifyFlags, kReadBarrierOption>(
              Runtime::Current()->GetClassLinker()->GetImagePointerSize())
          : klass->GetFirstReferenceInstanceFieldOffset<kVerifyFlags, kReadBarrierOption>();
      for (size_t i = 0u; i < num_reference_fields; ++i) {
        // TODO: Do a simpler check?
        if (field_offset.Uint32Value() != ClassOffset().Uint32Value()) {
          visitor(this, field_offset, kIsStatic);
        }
        field_offset = MemberOffset(field_offset.Uint32Value() +
                                    sizeof(mirror::HeapReference<mirror::Object>));
      }
    }
  }
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void Object::VisitInstanceFieldsReferences(mirror::Class* klass, const Visitor& visitor) {
  VisitFieldsReferences<false, kVerifyFlags, kReadBarrierOption>(
      klass->GetReferenceInstanceOffsets<kVerifyFlags>(), visitor);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void Object::VisitStaticFieldsReferences(mirror::Class* klass, const Visitor& visitor) {
  DCHECK(!klass->IsTemp());
  klass->VisitFieldsReferences<true, kVerifyFlags, kReadBarrierOption>(0, visitor);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsClassLoader() {
  return GetClass<kVerifyFlags, kReadBarrierOption>()->IsClassLoaderClass();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline mirror::ClassLoader* Object::AsClassLoader() {
  DCHECK((IsClassLoader<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<mirror::ClassLoader*>(this);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline bool Object::IsDexCache() {
  return GetClass<kVerifyFlags, kReadBarrierOption>()->IsDexCacheClass();
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline mirror::DexCache* Object::AsDexCache() {
  DCHECK((IsDexCache<kVerifyFlags, kReadBarrierOption>()));
  return down_cast<mirror::DexCache*>(this);
}

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor,
          typename JavaLangRefVisitor>
inline void Object::VisitReferences(const Visitor& visitor,
                                    const JavaLangRefVisitor& ref_visitor) {
  mirror::Class* klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  visitor(this, ClassOffset(), false);
  const uint32_t class_flags = klass->GetClassFlags<kVerifyNone>();
  if (LIKELY(class_flags == kClassFlagNormal)) {
    DCHECK((!klass->IsVariableSize<kVerifyFlags, kReadBarrierOption>()));
    VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
    DCHECK((!klass->IsClassClass<kVerifyFlags, kReadBarrierOption>()));
    DCHECK(!klass->IsStringClass());
    DCHECK(!klass->IsClassLoaderClass());
    DCHECK((!klass->IsArrayClass<kVerifyFlags, kReadBarrierOption>()));
  } else {
    if ((class_flags & kClassFlagNoReferenceFields) == 0) {
      DCHECK(!klass->IsStringClass());
      if (class_flags == kClassFlagClass) {
        DCHECK((klass->IsClassClass<kVerifyFlags, kReadBarrierOption>()));
        mirror::Class* as_klass = AsClass<kVerifyNone, kReadBarrierOption>();
        as_klass->VisitReferences<kVisitNativeRoots, kVerifyFlags, kReadBarrierOption>(klass,
                                                                                       visitor);
      } else if (class_flags == kClassFlagObjectArray) {
        DCHECK((klass->IsObjectArrayClass<kVerifyFlags, kReadBarrierOption>()));
        AsObjectArray<mirror::Object, kVerifyNone, kReadBarrierOption>()->VisitReferences(visitor);
      } else if ((class_flags & kClassFlagReference) != 0) {
        VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
        ref_visitor(klass, AsReference<kVerifyFlags, kReadBarrierOption>());
      } else if (class_flags == kClassFlagDexCache) {
        mirror::DexCache* const dex_cache = AsDexCache<kVerifyFlags, kReadBarrierOption>();
        dex_cache->VisitReferences<kVisitNativeRoots,
                                   kVerifyFlags,
                                   kReadBarrierOption>(klass, visitor);
      } else {
        mirror::ClassLoader* const class_loader = AsClassLoader<kVerifyFlags, kReadBarrierOption>();
        class_loader->VisitReferences<kVisitNativeRoots,
                                      kVerifyFlags,
                                      kReadBarrierOption>(klass, visitor);
      }
    } else if (kIsDebugBuild) {
      CHECK((!klass->IsClassClass<kVerifyFlags, kReadBarrierOption>()));
      CHECK((!klass->IsObjectArrayClass<kVerifyFlags, kReadBarrierOption>()));
      // String still has instance fields for reflection purposes but these don't exist in
      // actual string instances.
      if (!klass->IsStringClass()) {
        size_t total_reference_instance_fields = 0;
        mirror::Class* super_class = klass;
        do {
          total_reference_instance_fields += super_class->NumReferenceInstanceFields();
          super_class = super_class->GetSuperClass<kVerifyFlags, kReadBarrierOption>();
        } while (super_class != nullptr);
        // The only reference field should be the object's class. This field is handled at the
        // beginning of the function.
        CHECK_EQ(total_reference_instance_fields, 1u);
      }
    }
  }
}
}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_INL_H_
