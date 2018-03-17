/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_MIPS64_INSTRUCTION_SET_FEATURES_MIPS64_H_
#define ART_RUNTIME_ARCH_MIPS64_INSTRUCTION_SET_FEATURES_MIPS64_H_

#include "arch/instruction_set_features.h"

namespace art {

class Mips64InstructionSetFeatures;
using Mips64FeaturesUniquePtr = std::unique_ptr<const Mips64InstructionSetFeatures>;

// Instruction set features relevant to the MIPS64 architecture.
class Mips64InstructionSetFeatures FINAL : public InstructionSetFeatures {
 public:
  // Process a CPU variant string like "r4000" and create InstructionSetFeatures.
  static Mips64FeaturesUniquePtr FromVariant(const std::string& variant,
                                             std::string* error_msg);

  // Parse a bitmap and create an InstructionSetFeatures.
  static Mips64FeaturesUniquePtr FromBitmap(uint32_t bitmap);

  // Turn C pre-processor #defines into the equivalent instruction set features.
  static Mips64FeaturesUniquePtr FromCppDefines();

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static Mips64FeaturesUniquePtr FromCpuInfo();

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static Mips64FeaturesUniquePtr FromHwcap();

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static Mips64FeaturesUniquePtr FromAssembly();

  bool Equals(const InstructionSetFeatures* other) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE {
    return kMips64;
  }

  uint32_t AsBitmap() const OVERRIDE;

  std::string GetFeatureString() const OVERRIDE;

  // Does it have MSA (MIPS SIMD Architecture) support.
  bool HasMsa() const {
    return msa_;
  }

  virtual ~Mips64InstructionSetFeatures() {}

 protected:
  // Parse a vector of the form "fpu32", "mips2" adding these to a new Mips64InstructionSetFeatures.
  std::unique_ptr<const InstructionSetFeatures>
      AddFeaturesFromSplitString(const std::vector<std::string>& features,
                                 std::string* error_msg) const OVERRIDE;

 private:
  explicit Mips64InstructionSetFeatures(bool msa) : InstructionSetFeatures(), msa_(msa) {
  }

  // Bitmap positions for encoding features as a bitmap.
  enum {
    kMsaBitfield = 1,
  };

  const bool msa_;

  DISALLOW_COPY_AND_ASSIGN(Mips64InstructionSetFeatures);
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS64_INSTRUCTION_SET_FEATURES_MIPS64_H_
