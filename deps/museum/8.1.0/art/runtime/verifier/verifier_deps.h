/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_VERIFIER_DEPS_H_
#define ART_RUNTIME_VERIFIER_VERIFIER_DEPS_H_

#include <map>
#include <set>
#include <vector>

#include "base/array_ref.h"
#include "base/mutex.h"
#include "dex_file_types.h"
#include "handle.h"
#include "obj_ptr.h"
#include "thread.h"
#include "verifier_enums.h"  // For MethodVerifier::FailureKind.

namespace art {

class ArtField;
class ArtMethod;
class DexFile;
class VariableIndentationOutputStream;

namespace mirror {
class Class;
class ClassLoader;
}  // namespace mirror

namespace verifier {

// Verification dependencies collector class used by the MethodVerifier to record
// resolution outcomes and type assignability tests of classes/methods/fields
// not present in the set of compiled DEX files, that is classes/methods/fields
// defined in the classpath.
// The compilation driver initializes the class and registers all DEX files
// which are being compiled. Classes defined in DEX files outside of this set
// (or synthesized classes without associated DEX files) are considered being
// in the classpath.
// During code-flow verification, the MethodVerifier informs VerifierDeps
// about the outcome of every resolution and assignability test, and
// the VerifierDeps object records them if their outcome may change with
// changes in the classpath.
class VerifierDeps {
 public:
  explicit VerifierDeps(const std::vector<const DexFile*>& dex_files);

  VerifierDeps(const std::vector<const DexFile*>& dex_files, ArrayRef<const uint8_t> data);

  // Merge `other` into this `VerifierDeps`'. `other` and `this` must be for the
  // same set of dex files.
  void MergeWith(const VerifierDeps& other, const std::vector<const DexFile*>& dex_files);

  // Record the verification status of the class at `type_idx`.
  static void MaybeRecordVerificationStatus(const DexFile& dex_file,
                                            dex::TypeIndex type_idx,
                                            FailureKind failure_kind)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Record the outcome `klass` of resolving type `type_idx` from `dex_file`.
  // If `klass` is null, the class is assumed unresolved.
  static void MaybeRecordClassResolution(const DexFile& dex_file,
                                         dex::TypeIndex type_idx,
                                         mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Record the outcome `field` of resolving field `field_idx` from `dex_file`.
  // If `field` is null, the field is assumed unresolved.
  static void MaybeRecordFieldResolution(const DexFile& dex_file,
                                         uint32_t field_idx,
                                         ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Record the outcome `method` of resolving method `method_idx` from `dex_file`.
  // If `method` is null, the method is assumed unresolved.
  static void MaybeRecordMethodResolution(const DexFile& dex_file,
                                          uint32_t method_idx,
                                          ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Record the outcome `is_assignable` of type assignability test from `source`
  // to `destination` as defined by RegType::AssignableFrom. `dex_file` is the
  // owner of the method for which MethodVerifier performed the assignability test.
  static void MaybeRecordAssignability(const DexFile& dex_file,
                                       mirror::Class* destination,
                                       mirror::Class* source,
                                       bool is_strict,
                                       bool is_assignable)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Serialize the recorded dependencies and store the data into `buffer`.
  // `dex_files` provides the order of the dex files in which the dependencies
  // should be emitted.
  void Encode(const std::vector<const DexFile*>& dex_files, std::vector<uint8_t>* buffer) const;

  void Dump(VariableIndentationOutputStream* vios) const;

  // Verify the encoded dependencies of this `VerifierDeps` are still valid.
  bool ValidateDependencies(Handle<mirror::ClassLoader> class_loader, Thread* self) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  const std::set<dex::TypeIndex>& GetUnverifiedClasses(const DexFile& dex_file) const {
    return GetDexFileDeps(dex_file)->unverified_classes_;
  }

  bool OutputOnly() const {
    return output_only_;
  }

 private:
  static constexpr uint16_t kUnresolvedMarker = static_cast<uint16_t>(-1);

  using ClassResolutionBase = std::tuple<dex::TypeIndex, uint16_t>;
  struct ClassResolution : public ClassResolutionBase {
    ClassResolution() = default;
    ClassResolution(const ClassResolution&) = default;
    ClassResolution(dex::TypeIndex type_idx, uint16_t access_flags)
        : ClassResolutionBase(type_idx, access_flags) {}

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    dex::TypeIndex GetDexTypeIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
  };

  using FieldResolutionBase = std::tuple<uint32_t, uint16_t, dex::StringIndex>;
  struct FieldResolution : public FieldResolutionBase {
    FieldResolution() = default;
    FieldResolution(const FieldResolution&) = default;
    FieldResolution(uint32_t field_idx, uint16_t access_flags, dex::StringIndex declaring_class_idx)
        : FieldResolutionBase(field_idx, access_flags, declaring_class_idx) {}

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexFieldIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
    dex::StringIndex GetDeclaringClassIndex() const { return std::get<2>(*this); }
  };

  using MethodResolutionBase = std::tuple<uint32_t, uint16_t, dex::StringIndex>;
  struct MethodResolution : public MethodResolutionBase {
    MethodResolution() = default;
    MethodResolution(const MethodResolution&) = default;
    MethodResolution(uint32_t method_idx,
                     uint16_t access_flags,
                     dex::StringIndex declaring_class_idx)
        : MethodResolutionBase(method_idx, access_flags, declaring_class_idx) {}

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexMethodIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
    dex::StringIndex GetDeclaringClassIndex() const { return std::get<2>(*this); }
  };

  using TypeAssignabilityBase = std::tuple<dex::StringIndex, dex::StringIndex>;
  struct TypeAssignability : public TypeAssignabilityBase {
    TypeAssignability() = default;
    TypeAssignability(const TypeAssignability&) = default;
    TypeAssignability(dex::StringIndex destination_idx, dex::StringIndex source_idx)
        : TypeAssignabilityBase(destination_idx, source_idx) {}

    dex::StringIndex GetDestination() const { return std::get<0>(*this); }
    dex::StringIndex GetSource() const { return std::get<1>(*this); }
  };

  // Data structure representing dependencies collected during verification of
  // methods inside one DexFile.
  struct DexFileDeps {
    // Vector of strings which are not present in the corresponding DEX file.
    // These are referred to with ids starting with `NumStringIds()` of that DexFile.
    std::vector<std::string> strings_;

    // Set of class pairs recording the outcome of assignability test from one
    // of the two types to the other.
    std::set<TypeAssignability> assignable_types_;
    std::set<TypeAssignability> unassignable_types_;

    // Sets of recorded class/field/method resolutions.
    std::set<ClassResolution> classes_;
    std::set<FieldResolution> fields_;
    std::set<MethodResolution> methods_;

    // List of classes that were not fully verified in that dex file.
    std::set<dex::TypeIndex> unverified_classes_;

    bool Equals(const DexFileDeps& rhs) const;
  };

  VerifierDeps(const std::vector<const DexFile*>& dex_files, bool output_only);

  // Finds the DexFileDep instance associated with `dex_file`, or nullptr if
  // `dex_file` is not reported as being compiled.
  DexFileDeps* GetDexFileDeps(const DexFile& dex_file);

  const DexFileDeps* GetDexFileDeps(const DexFile& dex_file) const;

  // Returns true if `klass` is null or not defined in any of dex files which
  // were reported as being compiled.
  bool IsInClassPath(ObjPtr<mirror::Class> klass) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Finds the class in the classpath that makes `source` inherit` from `destination`.
  // Returns null if a class defined in the compiled DEX files, and assignable to
  // `source`, direclty inherits from `destination`.
  mirror::Class* FindOneClassPathBoundaryForInterface(mirror::Class* destination,
                                                      mirror::Class* source) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the index of `str`. If it is defined in `dex_file_`, this is the dex
  // string ID. If not, an ID is assigned to the string and cached in `strings_`
  // of the corresponding DexFileDeps structure (either provided or inferred from
  // `dex_file`).
  dex::StringIndex GetIdFromString(const DexFile& dex_file, const std::string& str)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Returns the string represented by `id`.
  std::string GetStringFromId(const DexFile& dex_file, dex::StringIndex string_id) const;

  // Returns the bytecode access flags of `element` (bottom 16 bits), or
  // `kUnresolvedMarker` if `element` is null.
  template <typename T>
  static uint16_t GetAccessFlags(T* element)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a string ID of the descriptor of the declaring class of `element`,
  // or `kUnresolvedMarker` if `element` is null.
  dex::StringIndex GetMethodDeclaringClassStringId(const DexFile& dex_file,
                                                   uint32_t dex_method_idx,
                                                   ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);
  dex::StringIndex GetFieldDeclaringClassStringId(const DexFile& dex_file,
                                                  uint32_t dex_field_idx,
                                                  ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a string ID of the descriptor of the class.
  dex::StringIndex GetClassDescriptorStringId(const DexFile& dex_file, ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  void AddClassResolution(const DexFile& dex_file,
                          dex::TypeIndex type_idx,
                          mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  void AddFieldResolution(const DexFile& dex_file,
                          uint32_t field_idx,
                          ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  void AddMethodResolution(const DexFile& dex_file,
                           uint32_t method_idx,
                           ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  void AddAssignability(const DexFile& dex_file,
                        mirror::Class* destination,
                        mirror::Class* source,
                        bool is_strict,
                        bool is_assignable)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool Equals(const VerifierDeps& rhs) const;

  // Verify `dex_file` according to the `deps`, that is going over each
  // `DexFileDeps` field, and checking that the recorded information still
  // holds.
  bool VerifyDexFile(Handle<mirror::ClassLoader> class_loader,
                     const DexFile& dex_file,
                     const DexFileDeps& deps,
                     Thread* self) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool VerifyAssignability(Handle<mirror::ClassLoader> class_loader,
                           const DexFile& dex_file,
                           const std::set<TypeAssignability>& assignables,
                           bool expected_assignability,
                           Thread* self) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Verify that the set of resolved classes at the point of creation
  // of this `VerifierDeps` is still the same.
  bool VerifyClasses(Handle<mirror::ClassLoader> class_loader,
                     const DexFile& dex_file,
                     const std::set<ClassResolution>& classes,
                     Thread* self) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Verify that the set of resolved fields at the point of creation
  // of this `VerifierDeps` is still the same, and each field resolves to the
  // same field holder and access flags.
  bool VerifyFields(Handle<mirror::ClassLoader> class_loader,
                    const DexFile& dex_file,
                    const std::set<FieldResolution>& classes,
                    Thread* self) const
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::verifier_deps_lock_);

  // Verify that the set of resolved methods at the point of creation
  // of this `VerifierDeps` is still the same, and each method resolves to the
  // same method holder, access flags, and invocation kind.
  bool VerifyMethods(Handle<mirror::ClassLoader> class_loader,
                     const DexFile& dex_file,
                     const std::set<MethodResolution>& methods,
                     Thread* self) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Map from DexFiles into dependencies collected from verification of their methods.
  std::map<const DexFile*, std::unique_ptr<DexFileDeps>> dex_deps_;

  // Output only signifies if we are using the verifier deps to verify or just to generate them.
  const bool output_only_;

  friend class VerifierDepsTest;
  ART_FRIEND_TEST(VerifierDepsTest, StringToId);
  ART_FRIEND_TEST(VerifierDepsTest, EncodeDecode);
  ART_FRIEND_TEST(VerifierDepsTest, EncodeDecodeMulti);
  ART_FRIEND_TEST(VerifierDepsTest, VerifyDeps);
  ART_FRIEND_TEST(VerifierDepsTest, CompilerDriver);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_DEPS_H_
