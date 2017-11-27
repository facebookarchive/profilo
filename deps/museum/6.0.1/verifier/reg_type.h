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

#include <stdint.h>
#include <limits>
#include <set>
#include <string>

#include "base/bit_vector.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "handle_scope.h"
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
  virtual bool IsUnresolvedAndUninitializedThisReference() const {
    return false;
  }
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
    return IsImpreciseConstant() || IsPreciseConstant();
  }
  bool IsConstantLo() const {
    return IsImpreciseConstantLo() || IsPreciseConstantLo();
  }
  bool IsPrecise() const {
    return IsPreciseConstantLo() || IsPreciseConstant() ||
           IsPreciseConstantHi();
  }
  bool IsLongConstant() const { return IsConstantLo(); }
  bool IsConstantHi() const {
    return (IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsLongConstantHigh() const { return IsConstantHi(); }
  virtual bool IsUninitializedTypes() const { return false; }
  virtual bool IsUnresolvedTypes() const { return false; }

  bool IsLowHalf() const {
    return (IsLongLo() || IsDoubleLo() || IsPreciseConstantLo() || IsImpreciseConstantLo());
  }
  bool IsHighHalf() const {
    return (IsLongHi() || IsDoubleHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsLongOrDoubleTypes() const { return IsLowHalf(); }
  // Check this is the low half, and that type_h is its matching high-half.
  inline bool CheckWidePair(const RegType& type_h) const {
    if (IsLowHalf()) {
      return ((IsImpreciseConstantLo() && type_h.IsPreciseConstantHi()) ||
              (IsImpreciseConstantLo() && type_h.IsImpreciseConstantHi()) ||
              (IsPreciseConstantLo() && type_h.IsPreciseConstantHi()) ||
              (IsPreciseConstantLo() && type_h.IsImpreciseConstantHi()) ||
              (IsDoubleLo() && type_h.IsDoubleHi()) ||
              (IsLongLo() && type_h.IsLongHi()));
    }
    return false;
  }
  // The high half that corresponds to this low half
  const RegType& HighHalf(RegTypeCache* cache) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsConstantBoolean() const;
  virtual bool IsConstantChar() const { return false; }
  virtual bool IsConstantByte() const { return false; }
  virtual bool IsConstantShort() const { return false; }
  virtual bool IsOne() const { return false; }
  virtual bool IsZero() const { return false; }
  bool IsReferenceTypes() const {
    return IsNonZeroReferenceTypes() || IsZero();
  }
  virtual bool IsNonZeroReferenceTypes() const { return false; }
  bool IsCategory1Types() const {
    return IsChar() || IsInteger() || IsFloat() || IsConstant() || IsByte() ||
           IsShort() || IsBoolean();
  }
  bool IsCategory2Types() const {
    return IsLowHalf();  // Don't expect explicit testing of high halves
  }
  bool IsBooleanTypes() const { return IsBoolean() || IsConstantBoolean(); }
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
    return IsInteger() || IsConstant() || IsByte() || IsShort() || IsChar() ||
           IsBoolean();
  }
  // Give the constant value encoded, but this shouldn't be called in the
  // general case.
  bool IsArrayIndexTypes() const { return IsIntegralTypes(); }
  // Float type may be derived from any constant type
  bool IsFloatTypes() const { return IsFloat() || IsConstant(); }
  bool IsLongTypes() const { return IsLongLo() || IsLongConstant(); }
  bool IsLongHighTypes() const {
    return (IsLongHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsDoubleTypes() const { return IsDoubleLo() || IsLongConstant(); }
  bool IsDoubleHighTypes() const {
    return (IsDoubleHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  virtual bool IsLong() const { return false; }
  bool HasClass() const {
    bool result = !klass_.IsNull();
    DCHECK_EQ(result, HasClassVirtual());
    return result;
  }
  virtual bool HasClassVirtual() const { return false; }
  bool IsJavaLangObject() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsArrayTypes() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsObjectArrayTypes() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  Primitive::Type GetPrimitiveType() const;
  bool IsJavaLangObjectArray() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsInstantiableTypes() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  const std::string& GetDescriptor() const {
    DCHECK(HasClass() ||
           (IsUnresolvedTypes() && !IsUnresolvedMergedReference() &&
            !IsUnresolvedSuperClass()));
    return descriptor_;
  }
  mirror::Class* GetClass() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    DCHECK(!IsUnresolvedReference());
    DCHECK(!klass_.IsNull()) << Dump();
    DCHECK(HasClass());
    return klass_.Read();
  }
  uint16_t GetId() const { return cache_id_; }
  const RegType& GetSuperClass(RegTypeCache* cache) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  virtual std::string Dump() const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) = 0;

  // Can this type access other?
  bool CanAccess(const RegType& other) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this type access a member with the given properties?
  bool CanAccessMember(mirror::Class* klass, uint32_t access_flags) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this type be assigned by src?
  // Note: Object and interface types may always be assigned to one another, see
  // comment on
  // ClassJoin.
  bool IsAssignableFrom(const RegType& src) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this array type potentially be assigned by src.
  // This function is necessary as array types are valid even if their components types are not,
  // e.g., when they component type could not be resolved. The function will return true iff the
  // types are assignable. It will return false otherwise. In case of return=false, soft_error
  // will be set to true iff the assignment test failure should be treated as a soft-error, i.e.,
  // when both array types have the same 'depth' and the 'final' component types may be assignable
  // (both are reference types).
  bool CanAssignArray(const RegType& src, RegTypeCache& reg_types,
                      Handle<mirror::ClassLoader> class_loader, bool* soft_error) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can this type be assigned by src? Variant of IsAssignableFrom that doesn't
  // allow assignment to
  // an interface from an Object.
  bool IsStrictlyAssignableFrom(const RegType& src) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Are these RegTypes the same?
  bool Equals(const RegType& other) const { return GetId() == other.GetId(); }

  // Compute the merge of this register from one edge (path) with incoming_type
  // from another.
  const RegType& Merge(const RegType& incoming_type, RegTypeCache* reg_types) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Same as above, but also handles the case where incoming_type == this.
  const RegType& SafeMerge(const RegType& incoming_type, RegTypeCache* reg_types) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (Equals(incoming_type)) {
      return *this;
    }
    return Merge(incoming_type, reg_types);
  }

  /*
   * A basic Join operation on classes. For a pair of types S and T the Join,
   *written S v T = J, is
   * S <: J, T <: J and for-all U such that S <: U, T <: U then J <: U. That is
   *J is the parent of
   * S and T such that there isn't a parent of both S and T that isn't also the
   *parent of J (ie J
   * is the deepest (lowest upper bound) parent of S and T).
   *
   * This operation applies for regular classes and arrays, however, for
   *interface types there
   * needn't be a partial ordering on the types. We could solve the problem of a
   *lack of a partial
   * order by introducing sets of types, however, the only operation permissible
   *on an interface is
   * invoke-interface. In the tradition of Java verifiers [1] we defer the
   *verification of interface
   * types until an invoke-interface call on the interface typed reference at
   *runtime and allow
   * the perversion of Object being assignable to an interface type (note,
   *however, that we don't
   * allow assignment of Object or Interface to any concrete class and are
   *therefore type safe).
   *
   * [1] Java bytecode verification: algorithms and formalizations, Xavier Leroy
   */
  static mirror::Class* ClassJoin(mirror::Class* s, mirror::Class* t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  virtual ~RegType() {}

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 protected:
  RegType(mirror::Class* klass, const std::string& descriptor,
          uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : descriptor_(descriptor), klass_(klass), cache_id_(cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const std::string descriptor_;
  mutable GcRoot<mirror::Class>
      klass_;  // Non-const only due to moving classes.
  const uint16_t cache_id_;

  friend class RegTypeCache;

 private:
  static bool AssignableFrom(const RegType& lhs, const RegType& rhs, bool strict)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  DISALLOW_COPY_AND_ASSIGN(RegType);
};

// Bottom type.
class ConflictType FINAL : public RegType {
 public:
  bool IsConflict() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the singleton Conflict instance.
  static const ConflictType* GetInstance() PURE;

  // Create the singleton instance.
  static const ConflictType* CreateInstance(mirror::Class* klass,
                                            const std::string& descriptor,
                                            uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Destroy the singleton instance.
  static void Destroy();

 private:
  ConflictType(mirror::Class* klass, const std::string& descriptor,
               uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {}

  static const ConflictType* instance_;
};

// A variant of the bottom type used to specify an undefined value in the
// incoming registers.
// Merging with UndefinedType yields ConflictType which is the true bottom.
class UndefinedType FINAL : public RegType {
 public:
  bool IsUndefined() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Get the singleton Undefined instance.
  static const UndefinedType* GetInstance() PURE;

  // Create the singleton instance.
  static const UndefinedType* CreateInstance(mirror::Class* klass,
                                             const std::string& descriptor,
                                             uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Destroy the singleton instance.
  static void Destroy();

 private:
  UndefinedType(mirror::Class* klass, const std::string& descriptor,
                uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {}

  static const UndefinedType* instance_;
};

class PrimitiveType : public RegType {
 public:
  PrimitiveType(mirror::Class* klass, const std::string& descriptor,
                uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool HasClassVirtual() const OVERRIDE { return true; }
};

class Cat1Type : public PrimitiveType {
 public:
  Cat1Type(mirror::Class* klass, const std::string& descriptor,
           uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class IntegerType : public Cat1Type {
 public:
  bool IsInteger() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const IntegerType* CreateInstance(mirror::Class* klass,
                                           const std::string& descriptor,
                                           uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const IntegerType* GetInstance() PURE;
  static void Destroy();

 private:
  IntegerType(mirror::Class* klass, const std::string& descriptor,
              uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {}
  static const IntegerType* instance_;
};

class BooleanType FINAL : public Cat1Type {
 public:
  bool IsBoolean() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const BooleanType* CreateInstance(mirror::Class* klass,
                                           const std::string& descriptor,
                                           uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const BooleanType* GetInstance() PURE;
  static void Destroy();

 private:
  BooleanType(mirror::Class* klass, const std::string& descriptor,
              uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {}

  static const BooleanType* instance_;
};

class ByteType FINAL : public Cat1Type {
 public:
  bool IsByte() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const ByteType* CreateInstance(mirror::Class* klass,
                                        const std::string& descriptor,
                                        uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const ByteType* GetInstance() PURE;
  static void Destroy();

 private:
  ByteType(mirror::Class* klass, const std::string& descriptor,
           uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {}
  static const ByteType* instance_;
};

class ShortType FINAL : public Cat1Type {
 public:
  bool IsShort() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const ShortType* CreateInstance(mirror::Class* klass,
                                         const std::string& descriptor,
                                         uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const ShortType* GetInstance() PURE;
  static void Destroy();

 private:
  ShortType(mirror::Class* klass, const std::string& descriptor,
            uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {}
  static const ShortType* instance_;
};

class CharType FINAL : public Cat1Type {
 public:
  bool IsChar() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const CharType* CreateInstance(mirror::Class* klass,
                                        const std::string& descriptor,
                                        uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const CharType* GetInstance() PURE;
  static void Destroy();

 private:
  CharType(mirror::Class* klass, const std::string& descriptor,
           uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {}
  static const CharType* instance_;
};

class FloatType FINAL : public Cat1Type {
 public:
  bool IsFloat() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const FloatType* CreateInstance(mirror::Class* klass,
                                         const std::string& descriptor,
                                         uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const FloatType* GetInstance() PURE;
  static void Destroy();

 private:
  FloatType(mirror::Class* klass, const std::string& descriptor,
            uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {}
  static const FloatType* instance_;
};

class Cat2Type : public PrimitiveType {
 public:
  Cat2Type(mirror::Class* klass, const std::string& descriptor,
           uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class LongLoType FINAL : public Cat2Type {
 public:
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsLongLo() const OVERRIDE { return true; }
  bool IsLong() const OVERRIDE { return true; }
  static const LongLoType* CreateInstance(mirror::Class* klass,
                                          const std::string& descriptor,
                                          uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const LongLoType* GetInstance() PURE;
  static void Destroy();

 private:
  LongLoType(mirror::Class* klass, const std::string& descriptor,
             uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {}
  static const LongLoType* instance_;
};

class LongHiType FINAL : public Cat2Type {
 public:
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsLongHi() const OVERRIDE { return true; }
  static const LongHiType* CreateInstance(mirror::Class* klass,
                                          const std::string& descriptor,
                                          uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const LongHiType* GetInstance() PURE;
  static void Destroy();

 private:
  LongHiType(mirror::Class* klass, const std::string& descriptor,
             uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {}
  static const LongHiType* instance_;
};

class DoubleLoType FINAL : public Cat2Type {
 public:
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool IsDoubleLo() const OVERRIDE { return true; }
  bool IsDouble() const OVERRIDE { return true; }
  static const DoubleLoType* CreateInstance(mirror::Class* klass,
                                            const std::string& descriptor,
                                            uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const DoubleLoType* GetInstance() PURE;
  static void Destroy();

 private:
  DoubleLoType(mirror::Class* klass, const std::string& descriptor,
               uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {}
  static const DoubleLoType* instance_;
};

class DoubleHiType FINAL : public Cat2Type {
 public:
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  virtual bool IsDoubleHi() const OVERRIDE { return true; }
  static const DoubleHiType* CreateInstance(mirror::Class* klass,
                                      const std::string& descriptor,
                                      uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static const DoubleHiType* GetInstance() PURE;
  static void Destroy();

 private:
  DoubleHiType(mirror::Class* klass, const std::string& descriptor,
               uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {}
  static const DoubleHiType* instance_;
};

class ConstantType : public RegType {
 public:
  ConstantType(uint32_t constant, uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(nullptr, "", cache_id), constant_(constant) {
  }


  // If this is a 32-bit constant, what is the value? This value may be
  // imprecise in which case
  // the value represents part of the integer range of values that may be held
  // in the register.
  int32_t ConstantValue() const {
    DCHECK(IsConstantTypes());
    return constant_;
  }

  int32_t ConstantValueLo() const {
    DCHECK(IsConstantLo());
    return constant_;
  }

  int32_t ConstantValueHi() const {
    if (IsConstantHi() || IsPreciseConstantHi() || IsImpreciseConstantHi()) {
      return constant_;
    } else {
      DCHECK(false);
      return 0;
    }
  }

  bool IsZero() const OVERRIDE {
    return IsPreciseConstant() && ConstantValue() == 0;
  }
  bool IsOne() const OVERRIDE {
    return IsPreciseConstant() && ConstantValue() == 1;
  }

  bool IsConstantChar() const OVERRIDE {
    return IsConstant() && ConstantValue() >= 0 &&
           ConstantValue() <= std::numeric_limits<uint16_t>::max();
  }
  bool IsConstantByte() const OVERRIDE {
    return IsConstant() &&
           ConstantValue() >= std::numeric_limits<int8_t>::min() &&
           ConstantValue() <= std::numeric_limits<int8_t>::max();
  }
  bool IsConstantShort() const OVERRIDE {
    return IsConstant() &&
           ConstantValue() >= std::numeric_limits<int16_t>::min() &&
           ConstantValue() <= std::numeric_limits<int16_t>::max();
  }
  virtual bool IsConstantTypes() const OVERRIDE { return true; }

 private:
  const uint32_t constant_;
};

class PreciseConstType FINAL : public ConstantType {
 public:
  PreciseConstType(uint32_t constant, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {}

  bool IsPreciseConstant() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class PreciseConstLoType FINAL : public ConstantType {
 public:
  PreciseConstLoType(uint32_t constant, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {}
  bool IsPreciseConstantLo() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class PreciseConstHiType FINAL : public ConstantType {
 public:
  PreciseConstHiType(uint32_t constant, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {}
  bool IsPreciseConstantHi() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class ImpreciseConstType FINAL : public ConstantType {
 public:
  ImpreciseConstType(uint32_t constat, uint16_t cache_id)
       SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
       : ConstantType(constat, cache_id) {
  }
  bool IsImpreciseConstant() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class ImpreciseConstLoType FINAL : public ConstantType {
 public:
  ImpreciseConstLoType(uint32_t constant, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {}
  bool IsImpreciseConstantLo() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class ImpreciseConstHiType FINAL : public ConstantType {
 public:
  ImpreciseConstHiType(uint32_t constant, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {}
  bool IsImpreciseConstantHi() const OVERRIDE { return true; }
  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Common parent of all uninitialized types. Uninitialized types are created by
// "new" dex
// instructions and must be passed to a constructor.
class UninitializedType : public RegType {
 public:
  UninitializedType(mirror::Class* klass, const std::string& descriptor,
                    uint32_t allocation_pc, uint16_t cache_id)
      : RegType(klass, descriptor, cache_id), allocation_pc_(allocation_pc) {}

  bool IsUninitializedTypes() const OVERRIDE;
  bool IsNonZeroReferenceTypes() const OVERRIDE;

  uint32_t GetAllocationPc() const {
    DCHECK(IsUninitializedTypes());
    return allocation_pc_;
  }

 private:
  const uint32_t allocation_pc_;
};

// Similar to ReferenceType but not yet having been passed to a constructor.
class UninitializedReferenceType FINAL : public UninitializedType {
 public:
  UninitializedReferenceType(mirror::Class* klass,
                             const std::string& descriptor,
                             uint32_t allocation_pc, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(klass, descriptor, allocation_pc, cache_id) {}

  bool IsUninitializedReference() const OVERRIDE { return true; }

  bool HasClassVirtual() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Similar to UnresolvedReferenceType but not yet having been passed to a
// constructor.
class UnresolvedUninitializedRefType FINAL : public UninitializedType {
 public:
  UnresolvedUninitializedRefType(const std::string& descriptor,
                                 uint32_t allocation_pc, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(nullptr, descriptor, allocation_pc, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedAndUninitializedReference() const OVERRIDE { return true; }

  bool IsUnresolvedTypes() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Similar to UninitializedReferenceType but special case for the this argument
// of a constructor.
class UninitializedThisReferenceType FINAL : public UninitializedType {
 public:
  UninitializedThisReferenceType(mirror::Class* klass,
                                 const std::string& descriptor,
                                 uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(klass, descriptor, 0, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  virtual bool IsUninitializedThisReference() const OVERRIDE { return true; }

  bool HasClassVirtual() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

class UnresolvedUninitializedThisRefType FINAL : public UninitializedType {
 public:
  UnresolvedUninitializedThisRefType(const std::string& descriptor,
                                     uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UninitializedType(nullptr, descriptor, 0, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedAndUninitializedThisReference() const OVERRIDE { return true; }

  bool IsUnresolvedTypes() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// A type of register holding a reference to an Object of type GetClass or a
// sub-class.
class ReferenceType FINAL : public RegType {
 public:
  ReferenceType(mirror::Class* klass, const std::string& descriptor,
                uint16_t cache_id) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {}

  bool IsReference() const OVERRIDE { return true; }

  bool IsNonZeroReferenceTypes() const OVERRIDE { return true; }

  bool HasClassVirtual() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// A type of register holding a reference to an Object of type GetClass and only
// an object of that
// type.
class PreciseReferenceType FINAL : public RegType {
 public:
  PreciseReferenceType(mirror::Class* klass, const std::string& descriptor,
                       uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  bool IsPreciseReference() const OVERRIDE { return true; }

  bool IsNonZeroReferenceTypes() const OVERRIDE { return true; }

  bool HasClassVirtual() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Common parent of unresolved types.
class UnresolvedType : public RegType {
 public:
  UnresolvedType(const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : RegType(nullptr, descriptor, cache_id) {}

  bool IsNonZeroReferenceTypes() const OVERRIDE;
};

// Similar to ReferenceType except the Class couldn't be loaded. Assignability
// and other tests made
// of this type must be conservative.
class UnresolvedReferenceType FINAL : public UnresolvedType {
 public:
  UnresolvedReferenceType(const std::string& descriptor, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UnresolvedType(descriptor, cache_id) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedReference() const OVERRIDE { return true; }

  bool IsUnresolvedTypes() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

// Type representing the super-class of an unresolved type.
class UnresolvedSuperClass FINAL : public UnresolvedType {
 public:
  UnresolvedSuperClass(uint16_t child_id, RegTypeCache* reg_type_cache,
                       uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : UnresolvedType("", cache_id),
        unresolved_child_id_(child_id),
        reg_type_cache_(reg_type_cache) {
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  bool IsUnresolvedSuperClass() const OVERRIDE { return true; }

  bool IsUnresolvedTypes() const OVERRIDE { return true; }

  uint16_t GetUnresolvedSuperClassChildId() const {
    DCHECK(IsUnresolvedSuperClass());
    return static_cast<uint16_t>(unresolved_child_id_ & 0xFFFF);
  }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const uint16_t unresolved_child_id_;
  const RegTypeCache* const reg_type_cache_;
};

// A merge of unresolved (and resolved) types. If the types were resolved this may be
// Conflict or another known ReferenceType.
class UnresolvedMergedType FINAL : public UnresolvedType {
 public:
  // Note: the constructor will copy the unresolved BitVector, not use it directly.
  UnresolvedMergedType(const RegType& resolved, const BitVector& unresolved,
                       const RegTypeCache* reg_type_cache, uint16_t cache_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // The resolved part. See description below.
  const RegType& GetResolvedPart() const {
    return resolved_part_;
  }
  // The unresolved part.
  const BitVector& GetUnresolvedTypes() const {
    return unresolved_types_;
  }

  bool IsUnresolvedMergedReference() const OVERRIDE { return true; }

  bool IsUnresolvedTypes() const OVERRIDE { return true; }

  std::string Dump() const OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const RegTypeCache* const reg_type_cache_;

  // The original implementation of merged types was a binary tree. Collection of the flattened
  // types ("leaves") can be expensive, so we store the expanded list now, as two components:
  // 1) A resolved component. We use Zero when there is no resolved component, as that will be
  //    an identity merge.
  // 2) A bitvector of the unresolved reference types. A bitvector was chosen with the assumption
  //    that there should not be too many types in flight in practice. (We also bias the index
  //    against the index of Zero, which is one of the later default entries in any cache.)
  const RegType& resolved_part_;
  const BitVector unresolved_types_;
};

std::ostream& operator<<(std::ostream& os, const RegType& rhs)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_H_
