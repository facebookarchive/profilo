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

#ifndef ART_RUNTIME_ARCH_INSTRUCTION_SET_FEATURES_H_
#define ART_RUNTIME_ARCH_INSTRUCTION_SET_FEATURES_H_

#include <ostream>
#include <vector>

#include "base/macros.h"
#include "instruction_set.h"

namespace art {

class ArmInstructionSetFeatures;
class Arm64InstructionSetFeatures;
class MipsInstructionSetFeatures;
class Mips64InstructionSetFeatures;
class X86InstructionSetFeatures;
class X86_64InstructionSetFeatures;

// Abstraction used to describe features of a different instruction sets.
class InstructionSetFeatures {
 public:
  // Process a CPU variant string for the given ISA and create an InstructionSetFeatures.
  static const InstructionSetFeatures* FromVariant(InstructionSet isa,
                                                   const std::string& variant,
                                                   std::string* error_msg);

  // Parse a bitmap for the given isa and create an InstructionSetFeatures.
  static const InstructionSetFeatures* FromBitmap(InstructionSet isa, uint32_t bitmap);

  // Turn C pre-processor #defines into the equivalent instruction set features for kRuntimeISA.
  static const InstructionSetFeatures* FromCppDefines();

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static const InstructionSetFeatures* FromCpuInfo();

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static const InstructionSetFeatures* FromHwcap();

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static const InstructionSetFeatures* FromAssembly();

  // Parse a string of the form "div,-atomic_ldrd_strd" adding and removing these features to
  // create a new InstructionSetFeatures.
  const InstructionSetFeatures* AddFeaturesFromString(const std::string& feature_list,
                                                      std::string* error_msg) const WARN_UNUSED;

  // Are these features the same as the other given features?
  virtual bool Equals(const InstructionSetFeatures* other) const = 0;

  // Return the ISA these features relate to.
  virtual InstructionSet GetInstructionSet() const = 0;

  // Return a bitmap that represents the features. ISA specific.
  virtual uint32_t AsBitmap() const = 0;

  // Return a string of the form "div,lpae" or "none".
  virtual std::string GetFeatureString() const = 0;

  // Does the instruction set variant require instructions for correctness with SMP?
  bool IsSmp() const {
    return smp_;
  }

  // Down cast this ArmInstructionFeatures.
  const ArmInstructionSetFeatures* AsArmInstructionSetFeatures() const;

  // Down cast this Arm64InstructionFeatures.
  const Arm64InstructionSetFeatures* AsArm64InstructionSetFeatures() const;

  // Down cast this MipsInstructionFeatures.
  const MipsInstructionSetFeatures* AsMipsInstructionSetFeatures() const;

  // Down cast this Mips64InstructionFeatures.
  const Mips64InstructionSetFeatures* AsMips64InstructionSetFeatures() const;

  // Down cast this X86InstructionFeatures.
  const X86InstructionSetFeatures* AsX86InstructionSetFeatures() const;

  // Down cast this X86_64InstructionFeatures.
  const X86_64InstructionSetFeatures* AsX86_64InstructionSetFeatures() const;

  virtual ~InstructionSetFeatures() {}

 protected:
  explicit InstructionSetFeatures(bool smp) : smp_(smp) {}

  // Returns true if variant appears in the array variants.
  static bool FindVariantInArray(const char* const variants[], size_t num_variants,
                                 const std::string& variant);

  // Add architecture specific features in sub-classes.
  virtual const InstructionSetFeatures*
      AddFeaturesFromSplitString(bool smp, const std::vector<std::string>& features,
                                 std::string* error_msg) const = 0;

 private:
  const bool smp_;

  DISALLOW_COPY_AND_ASSIGN(InstructionSetFeatures);
};
std::ostream& operator<<(std::ostream& os, const InstructionSetFeatures& rhs);

}  // namespace art

#endif  // ART_RUNTIME_ARCH_INSTRUCTION_SET_FEATURES_H_
