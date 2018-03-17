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
#include "base/scoped_arena_containers.h"
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
class ScopedArenaAllocator;
class StringPiece;

namespace verifier {

class RegType;

// Use 8 bytes since that is the default arena allocator alignment.
static constexpr size_t kDefaultArenaBitVectorBytes = 8;

class RegTypeCache {
 public:
  explicit RegTypeCache(bool can_load_classes, ScopedArenaAllocator& arena);
  ~RegTypeCache();
  static void Init() SHARED_REQUIRES(Locks::mutator_lock_) {
    if (!RegTypeCache::primitive_initialized_) {
      CHECK_EQ(RegTypeCache::primitive_count_, 0);
      CreatePrimitiveAndSmallConstantTypes();
      CHECK_EQ(RegTypeCache::primitive_count_, kNumPrimitivesAndSmallConstants);
      RegTypeCache::primitive_initialized_ = true;
    }
  }
  static void ShutDown();
  const art::verifier::RegType& GetFromId(uint16_t id) const;
  const RegType& From(mirror::ClassLoader* loader, const char* descriptor, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  // Find a RegType, returns null if not found.
  const RegType* FindClass(mirror::Class* klass, bool precise) const
      SHARED_REQUIRES(Locks::mutator_lock_);
  // Insert a new class with a specified descriptor, must not already be in the cache.
  const RegType* InsertClass(const StringPiece& descriptor, mirror::Class* klass, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  // Get or insert a reg type for a description, klass, and precision.
  const RegType& FromClass(const char* descriptor, mirror::Class* klass, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const ConstantType& FromCat1Const(int32_t value, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const ConstantType& FromCat2ConstLo(int32_t value, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const ConstantType& FromCat2ConstHi(int32_t value, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& FromDescriptor(mirror::ClassLoader* loader, const char* descriptor, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& FromUnresolvedMerge(const RegType& left, const RegType& right)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& FromUnresolvedSuperClass(const RegType& child)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const ConstantType& Zero() SHARED_REQUIRES(Locks::mutator_lock_) {
    return FromCat1Const(0, true);
  }
  const ConstantType& One() SHARED_REQUIRES(Locks::mutator_lock_) {
    return FromCat1Const(1, true);
  }
  size_t GetCacheSize() {
    return entries_.size();
  }
  const BooleanType& Boolean() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *BooleanType::GetInstance();
  }
  const ByteType& Byte() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *ByteType::GetInstance();
  }
  const CharType& Char() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *CharType::GetInstance();
  }
  const ShortType& Short() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *ShortType::GetInstance();
  }
  const IntegerType& Integer() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *IntegerType::GetInstance();
  }
  const FloatType& Float() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *FloatType::GetInstance();
  }
  const LongLoType& LongLo() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *LongLoType::GetInstance();
  }
  const LongHiType& LongHi() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *LongHiType::GetInstance();
  }
  const DoubleLoType& DoubleLo() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *DoubleLoType::GetInstance();
  }
  const DoubleHiType& DoubleHi() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *DoubleHiType::GetInstance();
  }
  const UndefinedType& Undefined() SHARED_REQUIRES(Locks::mutator_lock_) {
    return *UndefinedType::GetInstance();
  }
  const ConflictType& Conflict() {
    return *ConflictType::GetInstance();
  }

  const PreciseReferenceType& JavaLangClass() SHARED_REQUIRES(Locks::mutator_lock_);
  const PreciseReferenceType& JavaLangString() SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& JavaLangThrowable(bool precise) SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& JavaLangObject(bool precise) SHARED_REQUIRES(Locks::mutator_lock_);

  const UninitializedType& Uninitialized(const RegType& type, uint32_t allocation_pc)
      SHARED_REQUIRES(Locks::mutator_lock_);
  // Create an uninitialized 'this' argument for the given type.
  const UninitializedType& UninitializedThisArgument(const RegType& type)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& FromUninitialized(const RegType& uninit_type)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const ImpreciseConstType& ByteConstant() SHARED_REQUIRES(Locks::mutator_lock_);
  const ImpreciseConstType& CharConstant() SHARED_REQUIRES(Locks::mutator_lock_);
  const ImpreciseConstType& ShortConstant() SHARED_REQUIRES(Locks::mutator_lock_);
  const ImpreciseConstType& IntConstant() SHARED_REQUIRES(Locks::mutator_lock_);
  const ImpreciseConstType& PosByteConstant() SHARED_REQUIRES(Locks::mutator_lock_);
  const ImpreciseConstType& PosShortConstant() SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& GetComponentType(const RegType& array, mirror::ClassLoader* loader)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void Dump(std::ostream& os) SHARED_REQUIRES(Locks::mutator_lock_);
  const RegType& RegTypeFromPrimitiveType(Primitive::Type) const;

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info)
      SHARED_REQUIRES(Locks::mutator_lock_);
  static void VisitStaticRoots(RootVisitor* visitor)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  void FillPrimitiveAndSmallConstantTypes() SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::Class* ResolveClass(const char* descriptor, mirror::ClassLoader* loader)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool MatchDescriptor(size_t idx, const StringPiece& descriptor, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);
  const ConstantType& FromCat1NonSmallConstant(int32_t value, bool precise)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns the pass in RegType.
  template <class RegTypeType>
  RegTypeType& AddEntry(RegTypeType* new_entry) SHARED_REQUIRES(Locks::mutator_lock_);

  // Add a string piece to the arena allocator so that it stays live for the lifetime of the
  // verifier.
  StringPiece AddString(const StringPiece& string_piece);

  template <class Type>
  static const Type* CreatePrimitiveTypeInstance(const std::string& descriptor)
      SHARED_REQUIRES(Locks::mutator_lock_);
  static void CreatePrimitiveAndSmallConstantTypes() SHARED_REQUIRES(Locks::mutator_lock_);

  // A quick look up for popular small constants.
  static constexpr int32_t kMinSmallConstant = -1;
  static constexpr int32_t kMaxSmallConstant = 4;
  static const PreciseConstType* small_precise_constants_[kMaxSmallConstant -
                                                          kMinSmallConstant + 1];

  static constexpr size_t kNumPrimitivesAndSmallConstants =
      12 + (kMaxSmallConstant - kMinSmallConstant + 1);

  // Have the well known global primitives been created?
  static bool primitive_initialized_;

  // Number of well known primitives that will be copied into a RegTypeCache upon construction.
  static uint16_t primitive_count_;

  // The actual storage for the RegTypes.
  ScopedArenaVector<const RegType*> entries_;

  // Fast lookup for quickly finding entries that have a matching class.
  ScopedArenaVector<std::pair<GcRoot<mirror::Class>, const RegType*>> klass_entries_;

  // Whether or not we're allowed to load classes.
  const bool can_load_classes_;

  // Arena allocator.
  ScopedArenaAllocator& arena_;

  DISALLOW_COPY_AND_ASSIGN(RegTypeCache);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_
