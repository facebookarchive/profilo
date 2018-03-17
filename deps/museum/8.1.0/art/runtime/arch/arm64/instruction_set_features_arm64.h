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

#ifndef ART_RUNTIME_ARCH_ARM64_INSTRUCTION_SET_FEATURES_ARM64_H_
#define ART_RUNTIME_ARCH_ARM64_INSTRUCTION_SET_FEATURES_ARM64_H_

#include "arch/instruction_set_features.h"

namespace art {

class Arm64InstructionSetFeatures;
using Arm64FeaturesUniquePtr = std::unique_ptr<const Arm64InstructionSetFeatures>;

// Instruction set features relevant to the ARM64 architecture.
class Arm64InstructionSetFeatures FINAL : public InstructionSetFeatures {
 public:
  // Process a CPU variant string like "krait" or "cortex-a15" and create InstructionSetFeatures.
  static Arm64FeaturesUniquePtr FromVariant(const std::string& variant, std::string* error_msg);

  // Parse a bitmap and create an InstructionSetFeatures.
  static Arm64FeaturesUniquePtr FromBitmap(uint32_t bitmap);

  // Turn C pre-processor #defines into the equivalent instruction set features.
  static Arm64FeaturesUniquePtr FromCppDefines();

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static Arm64FeaturesUniquePtr FromCpuInfo();

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static Arm64FeaturesUniquePtr FromHwcap();

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static Arm64FeaturesUniquePtr FromAssembly();

  bool Equals(const InstructionSetFeatures* other) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE {
    return kArm64;
  }

  uint32_t AsBitmap() const OVERRIDE;

  // Return a string of the form "a53" or "none".
  std::string GetFeatureString() const OVERRIDE;

  // Generate code addressing Cortex-A53 erratum 835769?
  bool NeedFixCortexA53_835769() const {
      return fix_cortex_a53_835769_;
  }

  // Generate code addressing Cortex-A53 erratum 843419?
  bool NeedFixCortexA53_843419() const {
      return fix_cortex_a53_843419_;
  }

  virtual ~Arm64InstructionSetFeatures() {}

 protected:
  // Parse a vector of the form "a53" adding these to a new ArmInstructionSetFeatures.
  std::unique_ptr<const InstructionSetFeatures>
      AddFeaturesFromSplitString(const std::vector<std::string>& features,
                                 std::string* error_msg) const OVERRIDE;

 private:
  Arm64InstructionSetFeatures(bool needs_a53_835769_fix, bool needs_a53_843419_fix)
      : InstructionSetFeatures(),
        fix_cortex_a53_835769_(needs_a53_835769_fix),
        fix_cortex_a53_843419_(needs_a53_843419_fix) {
  }

  // Bitmap positions for encoding features as a bitmap.
  enum {
    kA53Bitfield = 1 << 0,
  };

  const bool fix_cortex_a53_835769_;
  const bool fix_cortex_a53_843419_;

  DISALLOW_COPY_AND_ASSIGN(Arm64InstructionSetFeatures);
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_ARM64_INSTRUCTION_SET_FEATURES_ARM64_H_
