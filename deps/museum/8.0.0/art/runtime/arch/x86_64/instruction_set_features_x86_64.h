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

#ifndef ART_RUNTIME_ARCH_X86_64_INSTRUCTION_SET_FEATURES_X86_64_H_
#define ART_RUNTIME_ARCH_X86_64_INSTRUCTION_SET_FEATURES_X86_64_H_

#include "arch/x86/instruction_set_features_x86.h"

namespace art {

class X86_64InstructionSetFeatures;
using X86_64FeaturesUniquePtr = std::unique_ptr<const X86_64InstructionSetFeatures>;

// Instruction set features relevant to the X86_64 architecture.
class X86_64InstructionSetFeatures FINAL : public X86InstructionSetFeatures {
 public:
  // Process a CPU variant string like "atom" or "nehalem" and create InstructionSetFeatures.
  static X86_64FeaturesUniquePtr FromVariant(const std::string& variant, std::string* error_msg) {
    return Convert(X86InstructionSetFeatures::FromVariant(variant, error_msg, true));
  }

  // Parse a bitmap and create an InstructionSetFeatures.
  static X86_64FeaturesUniquePtr FromBitmap(uint32_t bitmap) {
    return Convert(X86InstructionSetFeatures::FromBitmap(bitmap, true));
  }

  // Turn C pre-processor #defines into the equivalent instruction set features.
  static X86_64FeaturesUniquePtr FromCppDefines() {
    return Convert(X86InstructionSetFeatures::FromCppDefines(true));
  }

  // Process /proc/cpuinfo and use kRuntimeISA to produce InstructionSetFeatures.
  static X86_64FeaturesUniquePtr FromCpuInfo() {
    return Convert(X86InstructionSetFeatures::FromCpuInfo(true));
  }

  // Process the auxiliary vector AT_HWCAP entry and use kRuntimeISA to produce
  // InstructionSetFeatures.
  static X86_64FeaturesUniquePtr FromHwcap() {
    return Convert(X86InstructionSetFeatures::FromHwcap(true));
  }

  // Use assembly tests of the current runtime (ie kRuntimeISA) to determine the
  // InstructionSetFeatures. This works around kernel bugs in AT_HWCAP and /proc/cpuinfo.
  static X86_64FeaturesUniquePtr FromAssembly() {
    return Convert(X86InstructionSetFeatures::FromAssembly(true));
  }

  InstructionSet GetInstructionSet() const OVERRIDE {
    return kX86_64;
  }

  virtual ~X86_64InstructionSetFeatures() {}

 protected:
  // Parse a string of the form "ssse3" adding these to a new InstructionSetFeatures.
  std::unique_ptr<const InstructionSetFeatures>
      AddFeaturesFromSplitString(const std::vector<std::string>& features,
                                 std::string* error_msg) const OVERRIDE {
    return X86InstructionSetFeatures::AddFeaturesFromSplitString(features, true, error_msg);
  }

 private:
  X86_64InstructionSetFeatures(bool has_SSSE3,
                               bool has_SSE4_1,
                               bool has_SSE4_2,
                               bool has_AVX,
                               bool has_AVX2,
                               bool has_POPCNT)
      : X86InstructionSetFeatures(has_SSSE3, has_SSE4_1, has_SSE4_2, has_AVX,
                                  has_AVX2, has_POPCNT) {
  }

  static X86_64FeaturesUniquePtr Convert(X86FeaturesUniquePtr&& in) {
    return X86_64FeaturesUniquePtr(in.release()->AsX86_64InstructionSetFeatures());
  }

  friend class X86InstructionSetFeatures;

  DISALLOW_COPY_AND_ASSIGN(X86_64InstructionSetFeatures);
};

}  // namespace art

#endif  // ART_RUNTIME_ARCH_X86_64_INSTRUCTION_SET_FEATURES_X86_64_H_
