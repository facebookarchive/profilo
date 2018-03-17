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

#ifndef ART_RUNTIME_ARCH_MIPS_INSTRUCTION_SET_FEATURES_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_INSTRUCTION_SET_FEATURES_MIPS_H_

#include "arch/instruction_set_features.h"
#include "base/logging.h"
#include "base/macros.h"

namespace art {

class MipsInstructionSetFeatures;
using MipsFeaturesUniquePtr = std::unique_ptr<const MipsInstructionSetFeatures>;

// Instruction set features relevant to the MIPS architecture.
class MipsInstructionSetFeatures FINAL : public InstructionSetFeatures {
 public:
  // Process a CPU variant string like "r4000" and create InstructionSetFeatures.
  static MipsFeaturesUniquePtr FromVariant(const std::string& variant, std::string* error_msg);

  // Parse a bitmap and create an InstructionSetFeatures.
  static MipsFeaturesUniquePtr FromBitmap(uint32_t bitmap);

  // Turn C pre-processor #defines into the equivalent instruction set features.
  static MipsFeaturesUniquePtr FromCppDefines();

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static MipsFeaturesUniquePtr FromCpuInfo();

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static MipsFeaturesUniquePtr FromHwcap();

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static MipsFeaturesUniquePtr FromAssembly();

  bool Equals(const InstructionSetFeatures* other) const OVERRIDE;

  InstructionSet GetInstructionSet() const OVERRIDE {
    return kMips;
  }

  uint32_t AsBitmap() const OVERRIDE;

  std::string GetFeatureString() const OVERRIDE;

  // Is this an ISA revision greater than 2 opening up new opcodes.
  bool IsMipsIsaRevGreaterThanEqual2() const {
    return mips_isa_gte2_;
  }

  // Floating point double registers are encoded differently based on whether the Status.FR bit is
  // set. When the FR bit is 0 then the FPU is 32-bit, 1 its 64-bit. Return true if the code should
  // be generated assuming Status.FR is 0.
  bool Is32BitFloatingPoint() const {
    return fpu_32bit_;
  }

  bool IsR6() const {
    return r6_;
  }

  // Does it have MSA (MIPS SIMD Architecture) support.
  bool HasMsa() const {
    return msa_;
  }

  virtual ~MipsInstructionSetFeatures() {}

 protected:
  // Parse a vector of the form "fpu32", "mips2" adding these to a new MipsInstructionSetFeatures.
  std::unique_ptr<const InstructionSetFeatures>
      AddFeaturesFromSplitString(const std::vector<std::string>& features,
                                 std::string* error_msg) const OVERRIDE;

 private:
  MipsInstructionSetFeatures(bool fpu_32bit, bool mips_isa_gte2, bool r6, bool msa)
      : InstructionSetFeatures(),
        fpu_32bit_(fpu_32bit),
        mips_isa_gte2_(mips_isa_gte2),
        r6_(r6),
        msa_(msa) {
    // Sanity checks.
    if (r6) {
      CHECK(mips_isa_gte2);
      CHECK(!fpu_32bit);
    }
    if (!mips_isa_gte2) {
      CHECK(fpu_32bit);
    }
  }

  // Bitmap positions for encoding features as a bitmap.
  enum {
    kFpu32Bitfield = 1 << 0,
    kIsaRevGte2Bitfield = 1 << 1,
    kR6 = 1 << 2,
    kMsaBitfield = 1 << 3,
  };

  const bool fpu_32bit_;
  const bool mips_isa_gte2_;
  const bool r6_;
  const bool msa_;

  DISALLOW_COPY_AND_ASSIGN(MipsInstructionSetFeatures);
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS_INSTRUCTION_SET_FEATURES_MIPS_H_
