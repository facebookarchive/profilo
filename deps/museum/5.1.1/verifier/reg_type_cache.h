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

#ifndef ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_
#define ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_

#include "base/casts.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "object_callbacks.h"
#include "reg_type.h"
#include "runtime.h"

#include <stdint.h>
#include <vector>

namespace art {
namespace mirror {
  class Class;
  class ClassLoader;
}  // namespace mirror
class StringPiece;

namespace verifier {

class RegType;

class RegTypeCache {
 public:
  explicit RegTypeCache(bool can_load_classes);
  ~RegTypeCache();
  static void Init() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!RegTypeCache::primitive_initialized_) {
      CHECK_EQ(RegTypeCache::primitive_count_, 0);
      CreatePrimitiveAndSmallConstantTypes();
      CHECK_EQ(RegTypeCache::primitive_count_, kNumPrimitivesAndSmallConstants);
      RegTypeCache::primitive_initialized_ = true;
    }
  }
  static void ShutDown();
  RegType& GetFromId(uint16_t id) const;
  RegType& From(mirror::ClassLoader* loader, const char* descriptor, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& FromClass(const char* descriptor, mirror::Class* klass, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ConstantType& FromCat1Const(int32_t value, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ConstantType& FromCat2ConstLo(int32_t value, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ConstantType& FromCat2ConstHi(int32_t value, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& FromDescriptor(mirror::ClassLoader* loader, const char* descriptor, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& FromUnresolvedMerge(RegType& left, RegType& right)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& FromUnresolvedSuperClass(RegType& child)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& JavaLangString() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    // String is final and therefore always precise.
    return From(NULL, "Ljava/lang/String;", true);
  }
  RegType& JavaLangThrowable(bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return From(NULL, "Ljava/lang/Throwable;", precise);
  }
  ConstantType& Zero() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return FromCat1Const(0, true);
  }
  ConstantType& One() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return FromCat1Const(1, true);
  }
  size_t GetCacheSize() {
    return entries_.size();
  }
  static RegType& Boolean() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *BooleanType::GetInstance();
  }
  static RegType& Byte() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *ByteType::GetInstance();
  }
  static RegType& Char() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *CharType::GetInstance();
  }
  static RegType& Short() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *ShortType::GetInstance();
  }
  static RegType& Integer() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *IntegerType::GetInstance();
  }
  static RegType& Float() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *FloatType::GetInstance();
  }
  static RegType& LongLo() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *LongLoType::GetInstance();
  }
  static RegType& LongHi() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *LongHiType::GetInstance();
  }
  static RegType& DoubleLo() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *DoubleLoType::GetInstance();
  }
  static RegType& DoubleHi() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *DoubleHiType::GetInstance();
  }
  static RegType& Undefined() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return *UndefinedType::GetInstance();
  }
  static RegType& Conflict() {
    return *ConflictType::GetInstance();
  }
  RegType& JavaLangClass(bool precise) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return From(NULL, "Ljava/lang/Class;", precise);
  }
  RegType& JavaLangObject(bool precise) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return From(NULL, "Ljava/lang/Object;", precise);
  }
  UninitializedType& Uninitialized(RegType& type, uint32_t allocation_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  // Create an uninitialized 'this' argument for the given type.
  UninitializedType& UninitializedThisArgument(RegType& type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& FromUninitialized(RegType& uninit_type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ImpreciseConstType& ByteConstant() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ImpreciseConstType& CharConstant() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ImpreciseConstType& ShortConstant() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ImpreciseConstType& IntConstant() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ImpreciseConstType& PosByteConstant() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ImpreciseConstType& PosShortConstant() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& GetComponentType(RegType& array, mirror::ClassLoader* loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void Dump(std::ostream& os) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  RegType& RegTypeFromPrimitiveType(Primitive::Type) const;

  void VisitRoots(RootCallback* callback, void* arg) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void VisitStaticRoots(RootCallback* callback, void* arg)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  void FillPrimitiveAndSmallConstantTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  mirror::Class* ResolveClass(const char* descriptor, mirror::ClassLoader* loader)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool MatchDescriptor(size_t idx, const StringPiece& descriptor, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  ConstantType& FromCat1NonSmallConstant(int32_t value, bool precise)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void AddEntry(RegType* new_entry);

  template <class Type>
  static Type* CreatePrimitiveTypeInstance(const std::string& descriptor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void CreatePrimitiveAndSmallConstantTypes() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // The actual storage for the RegTypes.
  std::vector<RegType*> entries_;

  // A quick look up for popular small constants.
  static constexpr int32_t kMinSmallConstant = -1;
  static constexpr int32_t kMaxSmallConstant = 4;
  static PreciseConstType* small_precise_constants_[kMaxSmallConstant - kMinSmallConstant + 1];

  static constexpr size_t kNumPrimitivesAndSmallConstants =
      12 + (kMaxSmallConstant - kMinSmallConstant + 1);

  // Have the well known global primitives been created?
  static bool primitive_initialized_;

  // Number of well known primitives that will be copied into a RegTypeCache upon construction.
  static uint16_t primitive_count_;

  // Whether or not we're allowed to load classes.
  const bool can_load_classes_;

  DISALLOW_COPY_AND_ASSIGN(RegTypeCache);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_
