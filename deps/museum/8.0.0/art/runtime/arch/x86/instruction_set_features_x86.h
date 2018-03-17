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

#ifndef ART_RUNTIME_ARCH_X86_INSTRUCTION_SET_FEATURES_X86_H_
#define ART_RUNTIME_ARCH_X86_INSTRUCTION_SET_FEATURES_X86_H_

#include "arch/instruction_set_features.h"

namespace art {

class X86InstructionSetFeatures;
using X86FeaturesUniquePtr = std::unique_ptr<const X86InstructionSetFeatures>;

// Instruction set features relevant to the X86 architecture.
class X86InstructionSetFeatures : public InstructionSetFeatures {
 public:
  // Process a CPU variant string like "atom" or "nehalem" and create InstructionSetFeatures.
  static X86FeaturesUniquePtr FromVariant(const std::string& variant,
                                                                      std::string* error_msg,
                                                                      bool x86_64 = false);

  // Parse a bitmap and create an InstructionSetFeatures.
  static X86FeaturesUniquePtr FromBitmap(uint32_t bitmap,
                                                                     bool x86_64 = false);

  // Turn C pre-processor #defines into the equivalent instruction set features.
  static X86FeaturesUniquePtr FromCppDefines(bool x86_64 = false);

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static X86FeaturesUniquePtr FromCpuInfo(bool x86_64 = false);

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static X86FeaturesUniquePtr FromHwcap(bool x86_64 = false);

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static X86FeaturesUniquePtr FromAssembly(bool x86_64 = false);

  bool Equals(const InstructionSetFeatures* other) const OVERRIDE;

  virtual InstructionSet GetInstructionSet() const OVERRIDE {
    return kX86;
  }

  uint32_t AsBitmap() const OVERRIDE;

  std::string GetFeatureString() const OVERRIDE;

  virtual ~X86InstructionSetFeatures() {}

  bool HasSSE4_1() const { return has_SSE4_1_; }

  bool HasPopCnt() const { return has_POPCNT_; }

 protected:
  // Parse a string of the form "ssse3" adding these to a new InstructionSetFeatures.
  virtual std::unique_ptr<const InstructionSetFeatures>
      AddFeaturesFromSplitString(const std::vector<std::string>& features,
                                 std::string* error_msg) const OVERRIDE {
    return AddFeaturesFromSplitString(features, false, error_msg);
  }

  std::unique_ptr<const InstructionSetFeatures>
      AddFeaturesFromSplitString(const std::vector<std::string>& features,
                                 bool x86_64,
                                 std::string* error_msg) const;

  X86InstructionSetFeatures(bool has_SSSE3,
                            bool has_SSE4_1,
                            bool has_SSE4_2,
                            bool has_AVX,
                            bool has_AVX2,
                            bool has_POPCNT)
      : InstructionSetFeatures(),
        has_SSSE3_(has_SSSE3),
        has_SSE4_1_(has_SSE4_1),
        has_SSE4_2_(has_SSE4_2),
        has_AVX_(has_AVX),
        has_AVX2_(has_AVX2),
        has_POPCNT_(has_POPCNT) {
  }

  static X86FeaturesUniquePtr Create(bool x86_64,
                                     bool has_SSSE3,
                                     bool has_SSE4_1,
                                     bool has_SSE4_2,
                                     bool has_AVX,
                                     bool has_AVX2,
                                     bool has_POPCNT);

 private:
  // Bitmap positions for encoding features as a bitmap.
  enum {
    kSsse3Bitfield = 1 << 0,
    kSse4_1Bitfield = 1 << 1,
    kSse4_2Bitfield = 1 << 2,
    kAvxBitfield = 1 << 3,
    kAvx2Bitfield = 1 << 4,
    kPopCntBitfield = 1 << 5,
  };

  const bool has_SSSE3_;   // x86 128bit SIMD - Supplemental SSE.
  const bool has_SSE4_1_;  // x86 128bit SIMD SSE4.1.
  const bool has_SSE4_2_;  // x86 128bit SIMD SSE4.2.
  const bool has_AVX_;     // x86 256bit SIMD AVX.
  const bool has_AVX2_;    // x86 256bit SIMD AVX 2.0.
  const bool has_POPCNT_;  // x86 population count

  DISALLOW_COPY_AND_ASSIGN(X86InstructionSetFeatures);
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_INSTRUCTION_SET_FEATURES_X86_H_
