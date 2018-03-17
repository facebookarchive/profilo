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
#include "class-inl.h"
#include "class_flags.h"
#include "class_linker.h"
#include "class_loader-inl.h"
#include "dex_cache-inl.h"
#include "lock_word-inl.h"
#include "monitor.h"
#include "object_array-inl.h"
#include "object_reference-inl.h"
#include "object-readbarrier-inl.h"
#include "obj_ptr-inl.h"
#include "read_barrier-inl.h"
#include "reference.h"
#include "runtime.h"
#include "string-inl.h"
#include "throwable.h"

namespace art {
namespace mirror {

inline uint32_t Object::ClassSize(PointerSize pointer_size) {
  uint32_t vtable_entries = kVTableLength;
  return Class::ComputeClassSize(true, vtable_entries, 0, 0, 0, 0, 0, pointer_size);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline Class* Object::GetClass() {
  return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Object, klass_));
}

template<VerifyObjectFlags kVerifyFlags>
inline void Object::SetClass(ObjPtr<Class> new_klass) {
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

inline bool Object::CasLockWordWeakAcquire(LockWord old_val, LockWord new_val) {
  // Force use of non-transactional mode and do not check.
  return CasFieldWeakAcquire32<false, false>(
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

inline uint32_t Object::GetMarkBit() {
#ifdef USE_READ_BARRIER
  return GetLockWord(false).MarkBitState();
#else
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
#endif
}

inline void Object::SetReadBarrierState(uint32_t rb_state) {
  if (!kUseBakerReadBarrier) {
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }
  DCHECK(ReadBarrier::IsValidReadBarrierState(rb_state)) << rb_state;
  LockWord lw = GetLockWord(false);
  lw.SetReadBarrierState(rb_state);
  SetLockWord(lw, false);
}

inline void Object::AssertReadBarrierState() const {
  CHECK(kUseBakerReadBarrier);
  Object* obj = const_cast<Object*>(this);
  DCHECK(obj->GetReadBarrierState() == ReadBarrier::WhiteState())
      << "Bad Baker pointer: obj=" << reinterpret_cast<void*>(obj)
      << " rb_state" << reinterpret_cast<void*>(obj->GetReadBarrierState());
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::VerifierInstanceOf(ObjPtr<Class> klass) {
  DCHECK(klass != nullptr);
  DCHECK(GetClass<kVerifyFlags>() != nullptr);
  return klass->IsInterface() || InstanceOf(klass);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Object::InstanceOf(ObjPtr<Class> klass) {
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
  ObjPtr<Class> klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  ObjPtr<Class> component_type = klass->GetComponentType<kVerifyFlags, kReadBarrierOption>();
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
  ObjPtr<Class> klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  ObjPtr<Class> component_type = klass->GetComponentType<kVerifyFlags, kReadBarrierOption>();
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

template<VerifyObjectFlags kVerifyFlags>
inline size_t Object::SizeOf() {
  // Read barrier is never required for SizeOf since objects sizes are constant. Reading from-space
  // values is OK because of that.
  static constexpr ReadBarrierOption kReadBarrierOption = kWithoutReadBarrier;
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
      << " class=" << Class::PrettyClass(GetClass<kNewFlags, kReadBarrierOption>());
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
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
    REQUIRES_SHARED(Locks::mutator_lock_) {
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
inline bool Object::CasFieldWeakAcquire32(MemberOffset field_offset,
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

  return atomic_addr->CompareExchangeWeakAcquire(old_value, new_value);
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

template<typename kSize>
inline kSize Object::GetFieldAcquire(MemberOffset field_offset) {
  const uint8_t* raw_addr = reinterpret_cast<const uint8_t*>(this) + field_offset.Int32Value();
  const kSize* addr = reinterpret_cast<const kSize*>(raw_addr);
  return reinterpret_cast<const Atomic<kSize>*>(addr)->LoadAcquire();
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
                                                      ObjPtr<Object> new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    ObjPtr<Object> obj;
    if (kIsVolatile) {
      obj = GetFieldObjectVolatile<Object>(field_offset);
    } else {
      obj = GetFieldObject<Object>(field_offset);
    }
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, obj.Ptr(), true);
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
    objref_addr->Assign(new_value.Ptr());
    QuasiAtomic::ThreadFenceSequentiallyConsistent();
                                // Ensure this store occurs before any volatile loads.
  } else {
    objref_addr->Assign(new_value.Ptr());
  }
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags,
    bool kIsVolatile>
inline void Object::SetFieldObject(MemberOffset field_offset, ObjPtr<Object> new_value) {
  SetFieldObjectWithoutWriteBarrier<kTransactionActive, kCheckTransaction, kVerifyFlags,
      kIsVolatile>(field_offset, new_value);
  if (new_value != nullptr) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
    // TODO: Check field assignment could theoretically cause thread suspension, TODO: fix this.
    CheckFieldAssignment(field_offset, new_value);
  }
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline void Object::SetFieldObjectVolatile(MemberOffset field_offset, ObjPtr<Object> new_value) {
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
                                                             ObjPtr<Object> old_value,
                                                             ObjPtr<Object> new_value) {
  bool success = CasFieldWeakSequentiallyConsistentObjectWithoutWriteBarrier<
      kTransactionActive, kCheckTransaction, kVerifyFlags>(field_offset, old_value, new_value);
  if (success) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
  }
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakSequentiallyConsistentObjectWithoutWriteBarrier(
    MemberOffset field_offset,
    ObjPtr<Object> old_value,
    ObjPtr<Object> new_value) {
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
  HeapReference<Object> old_ref(HeapReference<Object>::FromObjPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromObjPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeWeakSequentiallyConsistent(old_ref.reference_,
                                                                        new_ref.reference_);
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongSequentiallyConsistentObject(MemberOffset field_offset,
                                                               ObjPtr<Object> old_value,
                                                               ObjPtr<Object> new_value) {
  bool success = CasFieldStrongSequentiallyConsistentObjectWithoutWriteBarrier<
      kTransactionActive, kCheckTransaction, kVerifyFlags>(field_offset, old_value, new_value);
  if (success) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
  }
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldStrongSequentiallyConsistentObjectWithoutWriteBarrier(
    MemberOffset field_offset,
    ObjPtr<Object> old_value,
    ObjPtr<Object> new_value) {
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
  HeapReference<Object> old_ref(HeapReference<Object>::FromObjPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromObjPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeStrongSequentiallyConsistent(old_ref.reference_,
                                                                          new_ref.reference_);
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakRelaxedObjectWithoutWriteBarrier(
    MemberOffset field_offset,
    ObjPtr<Object> old_value,
    ObjPtr<Object> new_value) {
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
  HeapReference<Object> old_ref(HeapReference<Object>::FromObjPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromObjPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeWeakRelaxed(old_ref.reference_,
                                                         new_ref.reference_);
  return success;
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasFieldWeakReleaseObjectWithoutWriteBarrier(
    MemberOffset field_offset,
    ObjPtr<Object> old_value,
    ObjPtr<Object> new_value) {
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
  HeapReference<Object> old_ref(HeapReference<Object>::FromObjPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromObjPtr(new_value));
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  Atomic<uint32_t>* atomic_addr = reinterpret_cast<Atomic<uint32_t>*>(raw_addr);

  bool success = atomic_addr->CompareExchangeWeakRelease(old_ref.reference_,
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
    for (ObjPtr<Class> klass = kIsStatic
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
inline void Object::VisitInstanceFieldsReferences(ObjPtr<Class> klass, const Visitor& visitor) {
  VisitFieldsReferences<false, kVerifyFlags, kReadBarrierOption>(
      klass->GetReferenceInstanceOffsets<kVerifyFlags>(), visitor);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption, typename Visitor>
inline void Object::VisitStaticFieldsReferences(ObjPtr<Class> klass, const Visitor& visitor) {
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

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_INL_H_
