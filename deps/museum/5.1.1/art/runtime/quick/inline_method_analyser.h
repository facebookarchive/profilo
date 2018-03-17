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

#ifndef ART_RUNTIME_QUICK_INLINE_METHOD_ANALYSER_H_
#define ART_RUNTIME_QUICK_INLINE_METHOD_ANALYSER_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "method_reference.h"

/*
 * NOTE: This code is part of the quick compiler. It lives in the runtime
 * only to allow the debugger to check whether a method has been inlined.
 */

namespace art {

namespace verifier {
class MethodVerifier;
}  // namespace verifier

enum InlineMethodOpcode : uint16_t {
  kIntrinsicDoubleCvt,
  kIntrinsicFloatCvt,
  kIntrinsicReverseBits,
  kIntrinsicReverseBytes,
  kIntrinsicAbsInt,
  kIntrinsicAbsLong,
  kIntrinsicAbsFloat,
  kIntrinsicAbsDouble,
  kIntrinsicMinMaxInt,
  kIntrinsicMinMaxLong,
  kIntrinsicMinMaxFloat,
  kIntrinsicMinMaxDouble,
  kIntrinsicSqrt,
  kIntrinsicCeil,
  kIntrinsicFloor,
  kIntrinsicRint,
  kIntrinsicRoundFloat,
  kIntrinsicRoundDouble,
  kIntrinsicReferenceGetReferent,
  kIntrinsicCharAt,
  kIntrinsicCompareTo,
  kIntrinsicIsEmptyOrLength,
  kIntrinsicIndexOf,
  kIntrinsicCurrentThread,
  kIntrinsicPeek,
  kIntrinsicPoke,
  kIntrinsicCas,
  kIntrinsicUnsafeGet,
  kIntrinsicUnsafePut,
  kIntrinsicSystemArrayCopyCharArray,

  kInlineOpNop,
  kInlineOpReturnArg,
  kInlineOpNonWideConst,
  kInlineOpIGet,
  kInlineOpIPut,
};
std::ostream& operator<<(std::ostream& os, const InlineMethodOpcode& rhs);

enum InlineMethodFlags : uint16_t {
  kNoInlineMethodFlags = 0x0000,
  kInlineIntrinsic     = 0x0001,
  kInlineSpecial       = 0x0002,
};

// IntrinsicFlags are stored in InlineMethod::d::raw_data
enum IntrinsicFlags {
  kIntrinsicFlagNone = 0,

  // kIntrinsicMinMaxInt
  kIntrinsicFlagMax = kIntrinsicFlagNone,
  kIntrinsicFlagMin = 1,

  // kIntrinsicIsEmptyOrLength
  kIntrinsicFlagLength  = kIntrinsicFlagNone,
  kIntrinsicFlagIsEmpty = kIntrinsicFlagMin,

  // kIntrinsicIndexOf
  kIntrinsicFlagBase0 = kIntrinsicFlagMin,

  // kIntrinsicUnsafeGet, kIntrinsicUnsafePut, kIntrinsicUnsafeCas
  kIntrinsicFlagIsLong     = kIntrinsicFlagMin,
  // kIntrinsicUnsafeGet, kIntrinsicUnsafePut
  kIntrinsicFlagIsVolatile = 2,
  // kIntrinsicUnsafePut, kIntrinsicUnsafeCas
  kIntrinsicFlagIsObject   = 4,
  // kIntrinsicUnsafePut
  kIntrinsicFlagIsOrdered  = 8,
};

struct InlineIGetIPutData {
  // The op_variant below is opcode-Instruction::IGET for IGETs and
  // opcode-Instruction::IPUT for IPUTs. This is because the runtime
  // doesn't know the OpSize enumeration.
  uint16_t op_variant : 3;
  uint16_t method_is_static : 1;
  uint16_t object_arg : 4;
  uint16_t src_arg : 4;  // iput only
  uint16_t return_arg_plus1 : 4;  // iput only, method argument to return + 1, 0 = return void.
  uint16_t field_idx;
  uint32_t is_volatile : 1;
  uint32_t field_offset : 31;
};
COMPILE_ASSERT(sizeof(InlineIGetIPutData) == sizeof(uint64_t), InvalidSizeOfInlineIGetIPutData);

struct InlineReturnArgData {
  uint16_t arg;
  uint16_t is_wide : 1;
  uint16_t is_object : 1;
  uint16_t reserved : 14;
  uint32_t reserved2;
};
COMPILE_ASSERT(sizeof(InlineReturnArgData) == sizeof(uint64_t), InvalidSizeOfInlineReturnArgData);

struct InlineMethod {
  InlineMethodOpcode opcode;
  InlineMethodFlags flags;
  union {
    uint64_t data;
    InlineIGetIPutData ifield_data;
    InlineReturnArgData return_data;
  } d;
};

class InlineMethodAnalyser {
 public:
  /**
   * Analyse method code to determine if the method is a candidate for inlining.
   * If it is, record the inlining data.
   *
   * @param verifier the method verifier holding data about the method to analyse.
   * @param method placeholder for the inline method data.
   * @return true if the method is a candidate for inlining, false otherwise.
   */
  static bool AnalyseMethodCode(verifier::MethodVerifier* verifier, InlineMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static constexpr bool IsInstructionIGet(Instruction::Code opcode) {
    return Instruction::IGET <= opcode && opcode <= Instruction::IGET_SHORT;
  }

  static constexpr bool IsInstructionIPut(Instruction::Code opcode) {
    return Instruction::IPUT <= opcode && opcode <= Instruction::IPUT_SHORT;
  }

  static constexpr uint16_t IGetVariant(Instruction::Code opcode) {
    return opcode - Instruction::IGET;
  }

  static constexpr uint16_t IPutVariant(Instruction::Code opcode) {
    return opcode - Instruction::IPUT;
  }

  // Determines whether the method is a synthetic accessor (method name starts with "access$").
  static bool IsSyntheticAccessor(MethodReference ref);

 private:
  static bool AnalyseReturnMethod(const DexFile::CodeItem* code_item, InlineMethod* result);
  static bool AnalyseConstMethod(const DexFile::CodeItem* code_item, InlineMethod* result);
  static bool AnalyseIGetMethod(verifier::MethodVerifier* verifier, InlineMethod* result)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static bool AnalyseIPutMethod(verifier::MethodVerifier* verifier, InlineMethod* result)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Can we fast path instance field access in a verified accessor?
  // If yes, computes field's offset and volatility and whether the method is static or not.
  static bool ComputeSpecialAccessorInfo(uint32_t field_idx, bool is_put,
                                         verifier::MethodVerifier* verifier,
                                         InlineIGetIPutData* result)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

}  // namespace art

#endif  // ART_RUNTIME_QUICK_INLINE_METHOD_ANALYSER_H_
