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

#ifndef ART_RUNTIME_VERIFIER_REG_TYPE_H_
#define ART_RUNTIME_VERIFIER_REG_TYPE_H_

#include <limits>
#include <stdint.h>
#include <set>
#include <string>

#include "jni.h"

#include "base/macros.h"
#include "gc_root.h"
#include "globals.h"
#include "object_callbacks.h"
#include "primitive.h"

namespace art {
namespace mirror {
class Class;
}  // namespace mirror
namespace verifier {

class RegTypeCache;
/*
 * RegType holds information about the "type" of data held in a register.
 */
class RegType {
 public:
  virtual bool IsUndefined() const { return false; }
  virtual bool IsConflict() const { return false; }
  virtual bool IsBoolean() const { return false; }
  virtual bool IsByte() const { return false; }
  virtual bool IsChar() const { return false; }
  virtual bool IsShort() const { return false; }
  virtual bool IsInteger() const { return false; }
  virtual bool IsLongLo() const { return false; }
  virtual bool IsLongHi() const { return false; }
  virtual bool IsFloat() const { return false; }
  virtual bool IsDouble() const { return false; }
  virtual bool IsDoubleLo() const { return false; }
  virtual bool IsDoubleHi() const { return false; }
  virtual bool IsUnresolvedReference() const { return false; }
  virtual bool IsUninitializedReference() const { return false; }
  virtual bool IsUninitializedThisReference() const { return false; }
  virtual bool IsUnresolvedAndUninitializedReference() const { return false; }
  virtual bool IsUnresolvedAndUninitializedThisReference() const { return false; }
  virtual bool IsUnresolvedMergedReference() const { return false; }
  virtual bool IsUnresolvedSuperClass() const { return false; }
  virtual bool IsReference() const { return false; }
  virtual bool IsPreciseReference() const { return false; }
  virtual bool IsPreciseConstant() const { return false; }
  virtual bool IsPreciseConstantLo() const { return false; }
  virtual bool IsPreciseConstantHi() const { return false; }
  virtual bool IsImpreciseConstantLo() const { return false; }
  virtual bool IsImpreciseConstantHi() const { return false; }
  virtual bool IsImpreciseConstant() const { return false; }
  virtual bool IsConstantTypes() const { return false; }
  bool IsConstant() const {
    return IsPreciseConstant() || IsImpreciseConstant();
  }
  bool IsConstantLo() const {
    return IsPreciseConstantLo() || IsImpreciseConstantLo();
  }
  bool IsPrecise() const {
    return IsPreciseConstantLo() || IsPreciseConstant() || IsPreciseConstantHi();
  }
  bool IsLongConstant() const {
    return IsConstantLo();
  }
  bool IsConstantHi() const {
    return (IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsLongConstantHigh() const {
    return IsConstantHi();
  }
  virtual bool IsUninitializedTypes() const { return false; }
  bool IsUnresolvedTypes() const {
    return IsUnresolvedReference() || IsUnresolvedAndUninitializedReference() ||
           IsUnresolvedAndUninitializedThisReference() ||
           IsUnresolvedMergedReference() || IsUnresolvedSuperClass();
  }

  bool IsLowHalf() const {
    return (IsLongLo() || IsDoubleLo() || IsPreciseConstantLo() ||
            IsImpreciseConstantLo());
  }
  bool IsHighHalf() const {
    return (IsLongHi() || IsDoubleHi() || IsPreciseConstantHi() ||
            IsImpreciseConstantHi());
  }
  bool IsLongOrDoubleTypes() const {
    return IsLowHalf();
  }
  // Check this is the low half, and that type_h is its matching high-half.
  inline bool CheckWidePair(RegType& type_h) const {
    if (IsLowHalf()) {
      return ((IsPreciseConstantLo() && type_h.IsPreciseConstantHi()) ||
              (IsPreciseConstantLo() && type_h.IsImpreciseConstantHi()) ||
              (IsImpreciseConstantLo() && type_h.IsPreciseConstantHi()) ||
              (IsImpreciseConstantLo() && type_h.IsImpreciseConstantHi()) ||
              (IsDoubleLo() && type_h.IsDoubleHi()) ||
              (IsLongLo() && type_h.IsLongHi()));
    }
    return false;
  }
  // The high half that corresponds to this low half
  RegType& HighHalf(RegTypeCache* cache) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsConstantBoolean() const {
    return IsConstant() && (ConstantValue() >= 0) && (ConstantValue() <= 1);
  }
  virtual bool IsConstantChar() const {
    return false;
  }
  virtual bool IsConstantByte() const {
    return false;
  }
  virtual bool IsConstantShort() const {
    return false;
  }
  virtual bool IsOne() const {
    return false;
  }
  virtual bool IsZero() const {
    return false;
  }
  bool IsReferenceTypes() const {
    return IsNonZeroReferenceTypes() || IsZero();
  }
  virtual bool IsNonZeroReferenceTypes() const {
    return false;
  }
  bool IsCategory1Types() const {
    return IsChar() || IsInteger() || IsFloat() || IsConstant() || IsByte() || IsShort() ||
        IsBoolean();
  }
  bool IsCategory2Types() const {
    return IsLowHalf();  // Don't expect explicit testing of high halves
  }
  bool IsBooleanTypes() const {
    return IsBoolean() || IsConstantBoolean();
  }
  bool IsByteTypes() const {
    return IsConstantByte() || IsByte() || IsBoolean();
  }
  bool IsShortTypes() const {
    return IsShort() || IsByte() || IsBoolean() || IsConstantShort();
  }
  bool IsCharTypes() const {
    return IsChar() || IsBooleanTypes() || IsConstantChar();
  }
  bool IsIntegralTypes() const {
    return IsInteger() || IsConstant() || IsByte() || IsShort() || IsChar() || IsBoolean();
  }
  // Give the constant value encoded, but this shouldn't be called in the general case.
  virtual int32_t ConstantValue() const;
  virtual int32_t ConstantValueLo() const;
  virtual int32_t ConstantValueHi() const;
  bool IsArrayIndexTypes() const {
    return IsIntegralTypes();
  }
  // Float type may be derived from any constant type
  bool IsFloatTypes() const {
    return IsFloat() || IsConstant();
  }
  bool IsLongTypes() const {
    return IsLongLo() || IsLongConstant();
  }
  bool IsLongHighTypes() const {
    return (IsLongHi() ||
            IsPreciseConstantHi() ||
            IsImpreciseConstantHi());
  }
  bool IsDoubleTypes() const {
    return IsDoubleLo() || IsLongConstant();
  }
  bool IsDoubleHighTypes() const {
    return (IsDoubleHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  virtual bool IsLong() const {
    return false;
  }
  virtual bool HasClass() const {
    return false;
  }
  bool IsJavaLangObject() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsArrayTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsObjectArrayTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  Primitive::Type GetPrimitiveType() const;
  bool IsJavaLangObjectArray() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsInstantiableTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  const std::string& GetDescriptor() const {
    DCHECK(HasClass() || (IsUnresolvedTypes() && !IsUnresolvedMergedReference() &&
                          !IsUnresolvedSuperClass()));
    return descriptor_;
  }
  mirror::Class* GetClass() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(!IsUnresolvedReference());
    DCHECK(!klass_.IsNull()) << Dump();
    DCHECK(HasClass());
    return klass_.Read();
  }
  uint16_t GetId() const {
    return cache_id_;
  }
  RegType& GetSuperClass(RegTypeCache* cache)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  virtual std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Can this type access other?
  bool CanAccess(RegType& other) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this type access a member with the given properties?
  bool CanAccessMember(mirror::Class* klass, uint32_t access_flags)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this type be assigned by src?
  // Note: Object and interface types may always be assigned to one another, see comment on
  // ClassJoin.
  bool IsAssignableFrom(RegType& src) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this type be assigned by src? Variant of IsAssignableFrom that doesn't allow assignment to
  // an interface from an Object.
  bool IsStrictlyAssignableFrom(RegType& src) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Are these RegTypes the same?
  bool Equals(RegType& other) const {
    return GetId() == other.GetId();
  }

  // Compute the merge of this register from one edge (path) with incoming_type from another.
  virtual RegType& Merge(RegType& incoming_type, RegTypeCache* reg_types)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * A basic Join operation on classes. For a pair of types S and T the Join, written S v T = J, is
   * S <: J, T <: J and for-all U such that S <: U, T <: U then J <: U. That is J is the parent of
   * S and T such that there isn't a parent of both S and T that isn't also the parent of J (ie J
   * is the deepest (lowest upper bound) parent of S and T).
   *
   * This operation applies for regular classes and arrays, however, for interface types there
   * needn't be a partial ordering on the types. We could solve the problem of a lack of a partial
   * order by introducing sets of types, however, the only operation permissible on an interface is
   * invoke-interface. In the tradition of Java verifiers [1] we defer the verification of interface
   * types until an invoke-interface call on the interface typed reference at runtime and allow
   * the perversion of Object being assignable to an interface type (note, however, that we don't
   * allow assignment of Object or Interface to any concrete class and are therefore type safe).
   *
   * [1] Java bytecode verification: algorithms and formalizations, Xavier Leroy
   */
  static mirror::Class* ClassJoin(mirror::Class* s, mirror::Class* t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  virtual ~RegType() {}

  void VisitRoots(RootCallback* callback, void* arg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 protected:
  RegType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : descriptor_(descriptor), klass_(GcRoot<mirror::Class>(klass)), cache_id_(cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);


  const std::string descriptor_;
  GcRoot<mirror::Class> klass_;
  const uint16_t cache_id_;

  friend class RegTypeCache;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegType);
};

// Bottom type.
class ConflictType : public RegType {
 public:
  bool IsConflict() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the singleton Conflict instance.
  static ConflictType* GetInstance();

  // Create the singleton instance.
  static ConflictType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                      uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Destroy the singleton instance.
  static void Destroy();

 private:
  ConflictType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {
  }

  static ConflictType* instance_;
};

// A variant of the bottom type used to specify an undefined value in the incoming registers.
// Merging with UndefinedType yields ConflictType which is the true bottom.
class UndefinedType : public RegType {
 public:
  bool IsUndefined() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the singleton Undefined instance.
  static UndefinedType* GetInstance();

  // Create the singleton instance.
  static UndefinedType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                       uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Destroy the singleton instance.
  static void Destroy();

 private:
  UndefinedType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {
  }

  virtual RegType& Merge(RegType& incoming_type, RegTypeCache* reg_types)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static UndefinedType* instance_;
};

class PrimitiveType : public RegType {
 public:
  PrimitiveType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class Cat1Type : public PrimitiveType {
 public:
  Cat1Type(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class IntegerType : public Cat1Type {
 public:
  bool IsInteger() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static IntegerType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                     uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static IntegerType* GetInstance();
  static void Destroy();
 private:
  IntegerType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
  }
  static IntegerType* instance_;
};

class BooleanType : public Cat1Type {
 public:
  bool IsBoolean() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static BooleanType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                     uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static BooleanType* GetInstance();
  static void Destroy();
 private:
  BooleanType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
  }

  static BooleanType* instance;
};

class ByteType : public Cat1Type {
 public:
  bool IsByte() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static ByteType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                  uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static ByteType* GetInstance();
  static void Destroy();
 private:
  ByteType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
  }
  static ByteType* instance_;
};

class ShortType : public Cat1Type {
 public:
  bool IsShort() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static ShortType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                   uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static ShortType* GetInstance();
  static void Destroy();
 private:
  ShortType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
  }
  static ShortType* instance_;
};

class CharType : public Cat1Type {
 public:
  bool IsChar() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static CharType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                  uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static CharType* GetInstance();
  static void Destroy();
 private:
  CharType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
  }
  static CharType* instance_;
};

class FloatType : public Cat1Type {
 public:
  bool IsFloat() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static FloatType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                   uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static FloatType* GetInstance();
  static void Destroy();
 private:
  FloatType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
  }
  static FloatType* instance_;
};

class Cat2Type : public PrimitiveType {
 public:
  Cat2Type(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class LongLoType : public Cat2Type {
 public:
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsLongLo() const {
    return true;
  }
  bool IsLong() const {
    return true;
  }
  static LongLoType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                    uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static LongLoType* GetInstance();
  static void Destroy();
 private:
  LongLoType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
  }
  static LongLoType* instance_;
};

class LongHiType : public Cat2Type {
 public:
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsLongHi() const {
    return true;
  }
  static LongHiType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                    uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static LongHiType* GetInstance();
  static void Destroy();
 private:
  LongHiType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
  }
  static LongHiType* instance_;
};

class DoubleLoType : public Cat2Type {
 public:
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsDoubleLo() const {
    return true;
  }
  bool IsDouble() const {
    return true;
  }
  static DoubleLoType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                      uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static DoubleLoType* GetInstance();
  static void Destroy();
 private:
  DoubleLoType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
  }
  static DoubleLoType* instance_;
};

class DoubleHiType : public Cat2Type {
 public:
  std::string Dump();
  virtual bool IsDoubleHi() const {
    return true;
  }
  static DoubleHiType* CreateInstance(mirror::Class* klass, const std::string& descriptor,
                                      uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static DoubleHiType* GetInstance();
  static void Destroy();
 private:
  DoubleHiType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
  }
  static DoubleHiType* instance_;
};

class ConstantType : public RegType {
 public:
  ConstantType(uint32_t constat, uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // If this is a 32-bit constant, what is the value? This value may be imprecise in which case
  // the value represents part of the integer range of values that may be held in the register.
  int32_t ConstantValue() const {
    DCHECK(IsConstantTypes());
    return constant_;
  }
  int32_t ConstantValueLo() const;
  int32_t ConstantValueHi() const;

  bool IsZero() const {
    return IsPreciseConstant() && ConstantValue() == 0;
  }
  bool IsOne() const {
    return IsPreciseConstant() && ConstantValue() == 1;
  }

  bool IsConstantChar() const {
    return IsConstant() && ConstantValue() >= 0 &&
           ConstantValue() <= std::numeric_limits<jchar>::max();
  }
  bool IsConstantByte() const {
    return IsConstant() &&
           ConstantValue() >= std::numeric_limits<jbyte>::min() &&
           ConstantValue() <= std::numeric_limits<jbyte>::max();
  }
  bool IsConstantShort() const {
    return IsConstant() &&
           ConstantValue() >= std::numeric_limits<jshort>::min() &&
           ConstantValue() <= std::numeric_limits<jshort>::max();
  }
  virtual bool IsConstantTypes() const { return true; }

 private:
  const uint32_t constant_;
};

class PreciseConstType : public ConstantType {
 public:
  PreciseConstType(uint32_t constat, uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constat, cache_id) {
  }

  bool IsPreciseConstant() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class PreciseConstLoType : public ConstantType {
 public:
  PreciseConstLoType(uint32_t constat, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constat, cache_id) {
  }
  bool IsPreciseConstantLo() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class PreciseConstHiType : public ConstantType {
 public:
  PreciseConstHiType(uint32_t constat, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constat, cache_id) {
  }
  bool IsPreciseConstantHi() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class ImpreciseConstType : public ConstantType {
 public:
  ImpreciseConstType(uint32_t constat, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsImpreciseConstant() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class ImpreciseConstLoType : public ConstantType {
 public:
  ImpreciseConstLoType(uint32_t constat, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) : ConstantType(constat, cache_id) {
  }
  bool IsImpreciseConstantLo() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class ImpreciseConstHiType : public ConstantType {
 public:
  ImpreciseConstHiType(uint32_t constat, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) : ConstantType(constat, cache_id) {
  }
  bool IsImpreciseConstantHi() const {
    return true;
  }
  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Common parent of all uninitialized types. Uninitialized types are created by "new" dex
// instructions and must be passed to a constructor.
class UninitializedType : public RegType {
 public:
  UninitializedType(mirror::Class* klass, const std::string& descriptor, uint32_t allocation_pc,
                    uint16_t cache_id)
      : RegType(klass, descriptor, cache_id), allocation_pc_(allocation_pc) {
  }

  bool IsUninitializedTypes() const;
  bool IsNonZeroReferenceTypes() const;

  uint32_t GetAllocationPc() const {
    DCHECK(IsUninitializedTypes());
    return allocation_pc_;
  }

 private:
  const uint32_t allocation_pc_;
};

// Similar to ReferenceType but not yet having been passed to a constructor.
class UninitializedReferenceType : public UninitializedType {
 public:
  UninitializedReferenceType(mirror::Class* klass, const std::string& descriptor,
                             uint32_t allocation_pc, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(klass, descriptor, allocation_pc, cache_id) {
  }

  bool IsUninitializedReference() const {
    return true;
  }

  bool HasClass() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Similar to UnresolvedReferenceType but not yet having been passed to a constructor.
class UnresolvedUninitializedRefType : public UninitializedType {
 public:
  UnresolvedUninitializedRefType(const std::string& descriptor, uint32_t allocation_pc,
                                 uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(NULL, descriptor, allocation_pc, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedAndUninitializedReference() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Similar to UninitializedReferenceType but special case for the this argument of a constructor.
class UninitializedThisReferenceType : public UninitializedType {
 public:
  UninitializedThisReferenceType(mirror::Class* klass, const std::string& descriptor,
                                 uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(klass, descriptor, 0, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  virtual bool IsUninitializedThisReference() const {
    return true;
  }

  bool HasClass() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class UnresolvedUninitializedThisRefType : public UninitializedType {
 public:
  UnresolvedUninitializedThisRefType(const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(NULL, descriptor, 0, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedAndUninitializedThisReference() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// A type of register holding a reference to an Object of type GetClass or a sub-class.
class ReferenceType : public RegType {
 public:
  ReferenceType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
     : RegType(klass, descriptor, cache_id) {
  }

  bool IsReference() const {
    return true;
  }

  bool IsNonZeroReferenceTypes() const {
    return true;
  }

  bool HasClass() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// A type of register holding a reference to an Object of type GetClass and only an object of that
// type.
class PreciseReferenceType : public RegType {
 public:
  PreciseReferenceType(mirror::Class* klass, const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsPreciseReference() const {
    return true;
  }

  bool IsNonZeroReferenceTypes() const {
    return true;
  }

  bool HasClass() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Common parent of unresolved types.
class UnresolvedType : public RegType {
 public:
  UnresolvedType(const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) : RegType(NULL, descriptor, cache_id) {
  }

  bool IsNonZeroReferenceTypes() const;
};

// Similar to ReferenceType except the Class couldn't be loaded. Assignability and other tests made
// of this type must be conservative.
class UnresolvedReferenceType : public UnresolvedType {
 public:
  UnresolvedReferenceType(const std::string& descriptor, uint16_t cache_id)
     SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) : UnresolvedType(descriptor, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedReference() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Type representing the super-class of an unresolved type.
class UnresolvedSuperClass : public UnresolvedType {
 public:
  UnresolvedSuperClass(uint16_t child_id, RegTypeCache* reg_type_cache, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UnresolvedType("", cache_id), unresolved_child_id_(child_id),
        reg_type_cache_(reg_type_cache) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedSuperClass() const {
    return true;
  }

  uint16_t GetUnresolvedSuperClassChildId() const {
    DCHECK(IsUnresolvedSuperClass());
    return static_cast<uint16_t>(unresolved_child_id_ & 0xFFFF);
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const uint16_t unresolved_child_id_;
  const RegTypeCache* const reg_type_cache_;
};

// A merge of two unresolved types. If the types were resolved this may be Conflict or another
// known ReferenceType.
class UnresolvedMergedType : public UnresolvedType {
 public:
  UnresolvedMergedType(uint16_t left_id, uint16_t right_id, const RegTypeCache* reg_type_cache,
                       uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UnresolvedType("", cache_id), reg_type_cache_(reg_type_cache), merged_types_(left_id, right_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  // The top of a tree of merged types.
  std::pair<uint16_t, uint16_t> GetTopMergedTypes() const {
    DCHECK(IsUnresolvedMergedReference());
    return merged_types_;
  }

  // The complete set of merged types.
  std::set<uint16_t> GetMergedTypes() const;

  bool IsUnresolvedMergedReference() const {
    return true;
  }

  std::string Dump() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const RegTypeCache* const reg_type_cache_;
  const std::pair<uint16_t, uint16_t> merged_types_;
};

std::ostream& operator<<(std::ostream& os, const RegType& rhs)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_H_
