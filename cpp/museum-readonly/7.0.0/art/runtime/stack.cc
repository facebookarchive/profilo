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

#include <museum/7.0.0/art/runtime/stack.h>

#include <museum/7.0.0/art/runtime/oat_file.h>
#include <museum/7.0.0/art/runtime/arch/context.h>
#include <museum/7.0.0/art/runtime/art_method-inl.h>
#include <museum/7.0.0/art/runtime/base/hex_dump.h>
#include <museum/7.0.0/art/runtime/entrypoints/entrypoint_utils-inl.h>
#include <museum/7.0.0/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/7.0.0/art/runtime/gc/space/image_space.h>
#include <museum/7.0.0/art/runtime/gc/space/space-inl.h>
#include <museum/7.0.0/art/runtime/jit/jit.h>
#include <museum/7.0.0/art/runtime/jit/jit_code_cache.h>
#include <museum/7.0.0/art/runtime/linear_alloc.h>
#include <museum/7.0.0/art/runtime/mirror/class-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object_array-inl.h>
#include <museum/7.0.0/art/runtime/oat_quick_method_header.h>
#include <museum/7.0.0/art/runtime/quick/quick_method_frame_info.h>
#include <museum/7.0.0/art/runtime/runtime.h>
#include <museum/7.0.0/art/runtime/thread.h>
#include <museum/7.0.0/art/runtime/thread_list.h>
#include <museum/7.0.0/art/runtime/verify_object-inl.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

static constexpr bool kDebugStackWalk = false;

mirror::Object* ShadowFrame::GetThisObject() const {
  ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return nullptr;
  } else if (m->IsNative()) {
    return GetVRegReference(0);
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    //CHECK(code_item != nullptr) << PrettyMethod(m);
    uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
    return GetVRegReference(reg);
  }
}

mirror::Object* ShadowFrame::GetThisObject(uint16_t num_ins) const {
  ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return nullptr;
  } else {
    return GetVRegReference(NumberOfVRegs() - num_ins);
  }
}

size_t ManagedStack::NumJniShadowFrameReferences() const {
  size_t count = 0;
  for (const ManagedStack* current_fragment = this; current_fragment != nullptr;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != nullptr;
         current_frame = current_frame->GetLink()) {
      if (current_frame->GetMethod()->IsNative()) {
        // The JNI ShadowFrame only contains references. (For indirect reference.)
        count += current_frame->NumberOfVRegs();
      }
    }
  }
  return count;
}

bool ManagedStack::ShadowFramesContain(StackReference<mirror::Object>* shadow_frame_entry) const {
  for (const ManagedStack* current_fragment = this; current_fragment != nullptr;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != nullptr;
         current_frame = current_frame->GetLink()) {
      if (current_frame->Contains(shadow_frame_entry)) {
        return true;
      }
    }
  }
  return false;
}

StackVisitor::StackVisitor(Thread* thread, Context* context, StackWalkKind walk_kind)
    : StackVisitor(thread, context, walk_kind, 0) {}

StackVisitor::StackVisitor(Thread* thread,
                           Context* context,
                           StackWalkKind walk_kind,
                           size_t num_frames)
    : thread_(thread),
      walk_kind_(walk_kind),
      cur_shadow_frame_(nullptr),
      cur_quick_frame_(nullptr),
      cur_quick_frame_pc_(0),
      cur_oat_quick_method_header_(nullptr),
      num_frames_(num_frames),
      cur_depth_(0),
      current_inlining_depth_(0),
      context_(context) {
  DCHECK(thread == Thread::Current() || thread->IsSuspended()) << *thread;
}

InlineInfo StackVisitor::GetCurrentInlineInfo() const {
  const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
  uint32_t native_pc_offset = method_header->NativeQuickPcOffset(cur_quick_frame_pc_);
  CodeInfo code_info = method_header->GetOptimizedCodeInfo();
  CodeInfoEncoding encoding = code_info.ExtractEncoding();
  StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset, encoding);
  DCHECK(stack_map.IsValid());
  return code_info.GetInlineInfoOf(stack_map, encoding);
}

ArtMethod* StackVisitor::GetMethod() const {
  if (cur_shadow_frame_ != nullptr) {
    return cur_shadow_frame_->GetMethod();
  } else if (cur_quick_frame_ != nullptr) {
    if (IsInInlinedFrame()) {
      size_t depth_in_stack_map = current_inlining_depth_ - 1;
      InlineInfo inline_info = GetCurrentInlineInfo();
      const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
      CodeInfoEncoding encoding = method_header->GetOptimizedCodeInfo().ExtractEncoding();
      DCHECK(walk_kind_ != StackWalkKind::kSkipInlinedFrames);
      bool allow_resolve = walk_kind_ != StackWalkKind::kIncludeInlinedFramesNoResolve;
      return allow_resolve
          ? GetResolvedMethod<true>(*GetCurrentQuickFrame(),
                                    inline_info,
                                    encoding.inline_info_encoding,
                                    depth_in_stack_map)
          : GetResolvedMethod<false>(*GetCurrentQuickFrame(),
                                     inline_info,
                                     encoding.inline_info_encoding,
                                     depth_in_stack_map);
    } else {
      return *cur_quick_frame_;
    }
  }
  return nullptr;
}

uint32_t StackVisitor::GetDexPc(bool abort_on_failure) const {
  if (cur_shadow_frame_ != nullptr) {
    return cur_shadow_frame_->GetDexPC();
  } else if (cur_quick_frame_ != nullptr) {
    if (IsInInlinedFrame()) {
      size_t depth_in_stack_map = current_inlining_depth_ - 1;
      const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
      CodeInfoEncoding encoding = method_header->GetOptimizedCodeInfo().ExtractEncoding();
      return GetCurrentInlineInfo().GetDexPcAtDepth(encoding.inline_info_encoding,
                                                    depth_in_stack_map);
    } else if (cur_oat_quick_method_header_ == nullptr) {
      return DexFile::kDexNoIndex;
    } else {
      return cur_oat_quick_method_header_->ToDexPc(
          GetMethod(), cur_quick_frame_pc_, abort_on_failure);
    }
  } else {
    return 0;
  }
}

extern "C" mirror::Object* artQuickGetProxyThisObject(ArtMethod** sp)
    SHARED_REQUIRES(Locks::mutator_lock_);

mirror::Object* StackVisitor::GetThisObject() const {
  DCHECK_EQ(Runtime::Current()->GetClassLinker()->GetImagePointerSize(), sizeof(void*));
  ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return nullptr;
  } else if (m->IsNative()) {
    if (cur_quick_frame_ != nullptr) {
      HandleScope* hs = reinterpret_cast<HandleScope*>(
          reinterpret_cast<char*>(cur_quick_frame_) + sizeof(ArtMethod*));
      return hs->GetReference(0);
    } else {
      return cur_shadow_frame_->GetVRegReference(0);
    }
  } else if (m->IsProxyMethod()) {
    if (cur_quick_frame_ != nullptr) {
      return artQuickGetProxyThisObject(cur_quick_frame_);
    } else {
      return cur_shadow_frame_->GetVRegReference(0);
    }
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    if (code_item == nullptr) {
      //UNIMPLEMENTED(ERROR) << "Failed to determine this object of abstract or proxy method: "
      //    << PrettyMethod(m);
      return nullptr;
    } else {
      uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
      uint32_t value = 0;
      bool success = GetVReg(m, reg, kReferenceVReg, &value);
      // We currently always guarantee the `this` object is live throughout the method.
      //CHECK(success) << "Failed to read the this object in " << PrettyMethod(m);
      return reinterpret_cast<mirror::Object*>(value);
    }
  }
}

size_t StackVisitor::GetNativePcOffset() const {
  DCHECK(!IsShadowFrame());
  return GetCurrentOatQuickMethodHeader()->NativeQuickPcOffset(cur_quick_frame_pc_);
}

bool StackVisitor::GetVRegFromDebuggerShadowFrame(uint16_t vreg,
                                                  VRegKind kind,
                                                  uint32_t* val) const {
  size_t frame_id = const_cast<StackVisitor*>(this)->GetFrameId();
  ShadowFrame* shadow_frame = thread_->FindDebuggerShadowFrame(frame_id);
  if (shadow_frame != nullptr) {
    bool* updated_vreg_flags = thread_->GetUpdatedVRegFlags(frame_id);
    DCHECK(updated_vreg_flags != nullptr);
    if (updated_vreg_flags[vreg]) {
      // Value is set by the debugger.
      if (kind == kReferenceVReg) {
        *val = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
            shadow_frame->GetVRegReference(vreg)));
      } else {
        *val = shadow_frame->GetVReg(vreg);
      }
      return true;
    }
  }
  // No value is set by the debugger.
  return false;
}

bool StackVisitor::GetVReg(ArtMethod* m, uint16_t vreg, VRegKind kind, uint32_t* val) const {
  if (cur_quick_frame_ != nullptr) {
    DCHECK(context_ != nullptr);  // You can't reliably read registers without a context.
    DCHECK(m == GetMethod());
    // Check if there is value set by the debugger.
    if (GetVRegFromDebuggerShadowFrame(vreg, kind, val)) {
      return true;
    }
    DCHECK(cur_oat_quick_method_header_->IsOptimized());
    return GetVRegFromOptimizedCode(m, vreg, kind, val);
  } else {
    DCHECK(cur_shadow_frame_ != nullptr);
    if (kind == kReferenceVReg) {
      *val = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
          cur_shadow_frame_->GetVRegReference(vreg)));
    } else {
      *val = cur_shadow_frame_->GetVReg(vreg);
    }
    return true;
  }
}

bool StackVisitor::GetVRegFromOptimizedCode(ArtMethod* m, uint16_t vreg, VRegKind kind,
                                            uint32_t* val) const {
  DCHECK_EQ(m, GetMethod());
  const DexFile::CodeItem* code_item = m->GetCodeItem();
  //DCHECK(code_item != nullptr) << PrettyMethod(m);  // Can't be null or how would we compile
                                                    // its instructions?
  uint16_t number_of_dex_registers = code_item->registers_size_;
  DCHECK_LT(vreg, code_item->registers_size_);
  const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
  CodeInfo code_info = method_header->GetOptimizedCodeInfo();
  CodeInfoEncoding encoding = code_info.ExtractEncoding();

  uint32_t native_pc_offset = method_header->NativeQuickPcOffset(cur_quick_frame_pc_);
  StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset, encoding);
  DCHECK(stack_map.IsValid());
  size_t depth_in_stack_map = current_inlining_depth_ - 1;

  DexRegisterMap dex_register_map = IsInInlinedFrame()
      ? code_info.GetDexRegisterMapAtDepth(depth_in_stack_map,
                                           code_info.GetInlineInfoOf(stack_map, encoding),
                                           encoding,
                                           number_of_dex_registers)
      : code_info.GetDexRegisterMapOf(stack_map, encoding, number_of_dex_registers);

  if (!dex_register_map.IsValid()) {
    return false;
  }
  DexRegisterLocation::Kind location_kind =
      dex_register_map.GetLocationKind(vreg, number_of_dex_registers, code_info, encoding);
  switch (location_kind) {
    case DexRegisterLocation::Kind::kInStack: {
      const int32_t offset = dex_register_map.GetStackOffsetInBytes(vreg,
                                                                    number_of_dex_registers,
                                                                    code_info,
                                                                    encoding);
      const uint8_t* addr = reinterpret_cast<const uint8_t*>(cur_quick_frame_) + offset;
      *val = *reinterpret_cast<const uint32_t*>(addr);
      return true;
    }
    case DexRegisterLocation::Kind::kInRegister:
    case DexRegisterLocation::Kind::kInRegisterHigh:
    case DexRegisterLocation::Kind::kInFpuRegister:
    case DexRegisterLocation::Kind::kInFpuRegisterHigh: {
      uint32_t reg =
          dex_register_map.GetMachineRegister(vreg, number_of_dex_registers, code_info, encoding);
      return GetRegisterIfAccessible(reg, kind, val);
    }
    case DexRegisterLocation::Kind::kConstant:
      *val = dex_register_map.GetConstant(vreg, number_of_dex_registers, code_info, encoding);
      return true;
    case DexRegisterLocation::Kind::kNone:
      return false;
    default:
      LOG(FATAL)
          << "Unexpected location kind "
          << dex_register_map.GetLocationInternalKind(vreg,
                                                      number_of_dex_registers,
                                                      code_info,
                                                      encoding);
      UNREACHABLE();
  }
}

bool StackVisitor::GetRegisterIfAccessible(uint32_t reg, VRegKind kind, uint32_t* val) const {
  const bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);

  // X86 float registers are 64-bit and the logic below does not apply.
  DCHECK(!is_float || kRuntimeISA != InstructionSet::kX86);

  if (!IsAccessibleRegister(reg, is_float)) {
    return false;
  }
  uintptr_t ptr_val = GetRegister(reg, is_float);
  const bool target64 = Is64BitInstructionSet(kRuntimeISA);
  if (target64) {
    const bool wide_lo = (kind == kLongLoVReg) || (kind == kDoubleLoVReg);
    const bool wide_hi = (kind == kLongHiVReg) || (kind == kDoubleHiVReg);
    int64_t value_long = static_cast<int64_t>(ptr_val);
    if (wide_lo) {
      ptr_val = static_cast<uintptr_t>(Low32Bits(value_long));
    } else if (wide_hi) {
      ptr_val = static_cast<uintptr_t>(High32Bits(value_long));
    }
  }
  *val = ptr_val;
  return true;
}

bool StackVisitor::GetVRegPairFromDebuggerShadowFrame(uint16_t vreg,
                                                      VRegKind kind_lo,
                                                      VRegKind kind_hi,
                                                      uint64_t* val) const {
  uint32_t low_32bits;
  uint32_t high_32bits;
  bool success = GetVRegFromDebuggerShadowFrame(vreg, kind_lo, &low_32bits);
  success &= GetVRegFromDebuggerShadowFrame(vreg + 1, kind_hi, &high_32bits);
  if (success) {
    *val = (static_cast<uint64_t>(high_32bits) << 32) | static_cast<uint64_t>(low_32bits);
  }
  return success;
}

bool StackVisitor::GetVRegPair(ArtMethod* m, uint16_t vreg, VRegKind kind_lo,
                               VRegKind kind_hi, uint64_t* val) const {
  if (kind_lo == kLongLoVReg) {
    DCHECK_EQ(kind_hi, kLongHiVReg);
  } else if (kind_lo == kDoubleLoVReg) {
    DCHECK_EQ(kind_hi, kDoubleHiVReg);
  } else {
    LOG(FATAL) << "Expected long or double: kind_lo=" << kind_lo << ", kind_hi=" << kind_hi;
    UNREACHABLE();
  }
  // Check if there is value set by the debugger.
  if (GetVRegPairFromDebuggerShadowFrame(vreg, kind_lo, kind_hi, val)) {
    return true;
  }
  if (cur_quick_frame_ != nullptr) {
    DCHECK(context_ != nullptr);  // You can't reliably read registers without a context.
    DCHECK(m == GetMethod());
    DCHECK(cur_oat_quick_method_header_->IsOptimized());
    return GetVRegPairFromOptimizedCode(m, vreg, kind_lo, kind_hi, val);
  } else {
    DCHECK(cur_shadow_frame_ != nullptr);
    *val = cur_shadow_frame_->GetVRegLong(vreg);
    return true;
  }
}

bool StackVisitor::GetVRegPairFromOptimizedCode(ArtMethod* m, uint16_t vreg,
                                                VRegKind kind_lo, VRegKind kind_hi,
                                                uint64_t* val) const {
  uint32_t low_32bits;
  uint32_t high_32bits;
  bool success = GetVRegFromOptimizedCode(m, vreg, kind_lo, &low_32bits);
  success &= GetVRegFromOptimizedCode(m, vreg + 1, kind_hi, &high_32bits);
  if (success) {
    *val = (static_cast<uint64_t>(high_32bits) << 32) | static_cast<uint64_t>(low_32bits);
  }
  return success;
}

bool StackVisitor::GetRegisterPairIfAccessible(uint32_t reg_lo, uint32_t reg_hi,
                                               VRegKind kind_lo, uint64_t* val) const {
  const bool is_float = (kind_lo == kDoubleLoVReg);
  if (!IsAccessibleRegister(reg_lo, is_float) || !IsAccessibleRegister(reg_hi, is_float)) {
    return false;
  }
  uintptr_t ptr_val_lo = GetRegister(reg_lo, is_float);
  uintptr_t ptr_val_hi = GetRegister(reg_hi, is_float);
  bool target64 = Is64BitInstructionSet(kRuntimeISA);
  if (target64) {
    int64_t value_long_lo = static_cast<int64_t>(ptr_val_lo);
    int64_t value_long_hi = static_cast<int64_t>(ptr_val_hi);
    ptr_val_lo = static_cast<uintptr_t>(Low32Bits(value_long_lo));
    ptr_val_hi = static_cast<uintptr_t>(High32Bits(value_long_hi));
  }
  *val = (static_cast<uint64_t>(ptr_val_hi) << 32) | static_cast<uint32_t>(ptr_val_lo);
  return true;
}

bool StackVisitor::SetVReg(ArtMethod* m,
                           uint16_t vreg,
                           uint32_t new_value,
                           VRegKind kind) {
  const DexFile::CodeItem* code_item = m->GetCodeItem();
  if (code_item == nullptr) {
    return false;
  }
  ShadowFrame* shadow_frame = GetCurrentShadowFrame();
  if (shadow_frame == nullptr) {
    // This is a compiled frame: we must prepare and update a shadow frame that will
    // be executed by the interpreter after deoptimization of the stack.
    const size_t frame_id = GetFrameId();
    const uint16_t num_regs = code_item->registers_size_;
    shadow_frame = thread_->FindOrCreateDebuggerShadowFrame(frame_id, num_regs, m, GetDexPc());
    CHECK(shadow_frame != nullptr);
    // Remember the vreg has been set for debugging and must not be overwritten by the
    // original value during deoptimization of the stack.
    thread_->GetUpdatedVRegFlags(frame_id)[vreg] = true;
  }
  if (kind == kReferenceVReg) {
    shadow_frame->SetVRegReference(vreg, reinterpret_cast<mirror::Object*>(new_value));
  } else {
    shadow_frame->SetVReg(vreg, new_value);
  }
  return true;
}

bool StackVisitor::SetVRegPair(ArtMethod* m,
                               uint16_t vreg,
                               uint64_t new_value,
                               VRegKind kind_lo,
                               VRegKind kind_hi) {
  if (kind_lo == kLongLoVReg) {
    DCHECK_EQ(kind_hi, kLongHiVReg);
  } else if (kind_lo == kDoubleLoVReg) {
    DCHECK_EQ(kind_hi, kDoubleHiVReg);
  } else {
    LOG(FATAL) << "Expected long or double: kind_lo=" << kind_lo << ", kind_hi=" << kind_hi;
    UNREACHABLE();
  }
  const DexFile::CodeItem* code_item = m->GetCodeItem();
  if (code_item == nullptr) {
    return false;
  }
  ShadowFrame* shadow_frame = GetCurrentShadowFrame();
  if (shadow_frame == nullptr) {
    // This is a compiled frame: we must prepare for deoptimization (see SetVRegFromDebugger).
    const size_t frame_id = GetFrameId();
    const uint16_t num_regs = code_item->registers_size_;
    shadow_frame = thread_->FindOrCreateDebuggerShadowFrame(frame_id, num_regs, m, GetDexPc());
    CHECK(shadow_frame != nullptr);
    // Remember the vreg pair has been set for debugging and must not be overwritten by the
    // original value during deoptimization of the stack.
    thread_->GetUpdatedVRegFlags(frame_id)[vreg] = true;
    thread_->GetUpdatedVRegFlags(frame_id)[vreg + 1] = true;
  }
  shadow_frame->SetVRegLong(vreg, new_value);
  return true;
}

bool StackVisitor::IsAccessibleGPR(uint32_t reg) const {
  DCHECK(context_ != nullptr);
  return context_->IsAccessibleGPR(reg);
}

uintptr_t* StackVisitor::GetGPRAddress(uint32_t reg) const {
  DCHECK(cur_quick_frame_ != nullptr) << "This is a quick frame routine";
  DCHECK(context_ != nullptr);
  return context_->GetGPRAddress(reg);
}

uintptr_t StackVisitor::GetGPR(uint32_t reg) const {
  DCHECK(cur_quick_frame_ != nullptr) << "This is a quick frame routine";
  DCHECK(context_ != nullptr);
  return context_->GetGPR(reg);
}

bool StackVisitor::IsAccessibleFPR(uint32_t reg) const {
  DCHECK(context_ != nullptr);
  return context_->IsAccessibleFPR(reg);
}

uintptr_t StackVisitor::GetFPR(uint32_t reg) const {
  DCHECK(cur_quick_frame_ != nullptr) << "This is a quick frame routine";
  DCHECK(context_ != nullptr);
  return context_->GetFPR(reg);
}

uintptr_t StackVisitor::GetReturnPc() const {
  uint8_t* sp = reinterpret_cast<uint8_t*>(GetCurrentQuickFrame());
  DCHECK(sp != nullptr);
  uint8_t* pc_addr = sp + GetCurrentQuickFrameInfo().GetReturnPcOffset();
  return *reinterpret_cast<uintptr_t*>(pc_addr);
}

void StackVisitor::SetReturnPc(uintptr_t new_ret_pc) {
  uint8_t* sp = reinterpret_cast<uint8_t*>(GetCurrentQuickFrame());
  CHECK(sp != nullptr);
  uint8_t* pc_addr = sp + GetCurrentQuickFrameInfo().GetReturnPcOffset();
  *reinterpret_cast<uintptr_t*>(pc_addr) = new_ret_pc;
}

size_t StackVisitor::ComputeNumFrames(Thread* thread, StackWalkKind walk_kind) {
  struct NumFramesVisitor : public StackVisitor {
    NumFramesVisitor(Thread* thread_in, StackWalkKind walk_kind_in)
        : StackVisitor(thread_in, nullptr, walk_kind_in), frames(0) {}

    bool VisitFrame() OVERRIDE {
      frames++;
      return true;
    }

    size_t frames;
  };
  NumFramesVisitor visitor(thread, walk_kind);
  visitor.WalkStack(true);
  return visitor.frames;
}

bool StackVisitor::GetNextMethodAndDexPc(ArtMethod** next_method, uint32_t* next_dex_pc) {
  struct HasMoreFramesVisitor : public StackVisitor {
    HasMoreFramesVisitor(Thread* thread,
                         StackWalkKind walk_kind,
                         size_t num_frames,
                         size_t frame_height)
        : StackVisitor(thread, nullptr, walk_kind, num_frames),
          frame_height_(frame_height),
          found_frame_(false),
          has_more_frames_(false),
          next_method_(nullptr),
          next_dex_pc_(0) {
    }

    bool VisitFrame() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_) {
      if (found_frame_) {
        ArtMethod* method = GetMethod();
        if (method != nullptr && !method->IsRuntimeMethod()) {
          has_more_frames_ = true;
          next_method_ = method;
          next_dex_pc_ = GetDexPc();
          return false;  // End stack walk once next method is found.
        }
      } else if (GetFrameHeight() == frame_height_) {
        found_frame_ = true;
      }
      return true;
    }

    size_t frame_height_;
    bool found_frame_;
    bool has_more_frames_;
    ArtMethod* next_method_;
    uint32_t next_dex_pc_;
  };
  HasMoreFramesVisitor visitor(thread_, walk_kind_, GetNumFrames(), GetFrameHeight());
  visitor.WalkStack(true);
  *next_method = visitor.next_method_;
  *next_dex_pc = visitor.next_dex_pc_;
  return visitor.has_more_frames_;
}

void StackVisitor::DescribeStack(Thread* thread) {
  struct DescribeStackVisitor : public StackVisitor {
    explicit DescribeStackVisitor(Thread* thread_in)
        : StackVisitor(thread_in, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames) {}

    bool VisitFrame() OVERRIDE SHARED_REQUIRES(Locks::mutator_lock_) {
      LOG(INFO) << "Frame Id=" << GetFrameId() << " " << DescribeLocation();
      return true;
    }
  };
  DescribeStackVisitor visitor(thread);
  visitor.WalkStack(true);
}

std::string StackVisitor::DescribeLocation() const {
  std::string result("Visiting method '");
  ArtMethod* m = GetMethod();
  if (m == nullptr) {
    return "upcall";
  }
  //result += PrettyMethod(m);
  //result += StringPrintf("' at dex PC 0x%04x", GetDexPc());
  //if (!IsShadowFrame()) {
  //  result += StringPrintf(" (native PC %p)", reinterpret_cast<void*>(GetCurrentQuickFramePc()));
  //}
  return result;
}

static instrumentation::InstrumentationStackFrame& GetInstrumentationStackFrame(Thread* thread,
                                                                                uint32_t depth) {
  CHECK_LT(depth, thread->GetInstrumentationStack()->size());
  return thread->GetInstrumentationStack()->at(depth);
}

static void AssertPcIsWithinQuickCode(ArtMethod* method, uintptr_t pc)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  if (method->IsNative() || method->IsRuntimeMethod() || method->IsProxyMethod()) {
    return;
  }

  if (pc == reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc())) {
    return;
  }

  const void* code = method->GetEntryPointFromQuickCompiledCode();
  if (code == GetQuickInstrumentationEntryPoint()) {
    return;
  }

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (class_linker->IsQuickToInterpreterBridge(code) ||
      class_linker->IsQuickResolutionStub(code)) {
    return;
  }

  // If we are the JIT then we may have just compiled the method after the
  // IsQuickToInterpreterBridge check.
  Runtime* runtime = Runtime::Current();
  if (runtime->UseJitCompilation() && runtime->GetJit()->GetCodeCache()->ContainsPc(code)) {
    return;
  }

  uint32_t code_size = OatQuickMethodHeader::FromEntryPoint(code)->code_size_;
  uintptr_t code_start = reinterpret_cast<uintptr_t>(code);
  //CHECK(code_start <= pc && pc <= (code_start + code_size))
  //    << PrettyMethod(method)
  //    << " pc=" << std::hex << pc
  //    << " code_start=" << code_start
  //    << " code_size=" << code_size;
}

void StackVisitor::SanityCheckFrame() const {
  if (kIsDebugBuild) {
    ArtMethod* method = GetMethod();
    auto* declaring_class = method->GetDeclaringClass();
    // Runtime methods have null declaring class.
    if (!method->IsRuntimeMethod()) {
      CHECK(declaring_class != nullptr);
      CHECK_EQ(declaring_class->GetClass(), declaring_class->GetClass()->GetClass())
          << declaring_class;
    } else {
      CHECK(declaring_class == nullptr);
    }
    Runtime* const runtime = Runtime::Current();
    LinearAlloc* const linear_alloc = runtime->GetLinearAlloc();
    if (!linear_alloc->Contains(method)) {
      // Check class linker linear allocs.
      mirror::Class* klass = method->GetDeclaringClass();
      LinearAlloc* const class_linear_alloc = (klass != nullptr)
          ? runtime->GetClassLinker()->GetAllocatorForClassLoader(klass->GetClassLoader())
          : linear_alloc;
      if (!class_linear_alloc->Contains(method)) {
        // Check image space.
        bool in_image = false;
        for (auto& space : runtime->GetHeap()->GetContinuousSpaces()) {
          if (space->IsImageSpace()) {
            auto* image_space = space->AsImageSpace();
            const auto& header = image_space->GetImageHeader();
            const ImageSection& methods = header.GetMethodsSection();
            const ImageSection& runtime_methods = header.GetRuntimeMethodsSection();
            const size_t offset =  reinterpret_cast<const uint8_t*>(method) - image_space->Begin();
            if (methods.Contains(offset) || runtime_methods.Contains(offset)) {
              in_image = true;
              break;
            }
          }
        }
        //CHECK(in_image) << PrettyMethod(method) << " not in linear alloc or image";
      }
    }
    if (cur_quick_frame_ != nullptr) {
      AssertPcIsWithinQuickCode(method, cur_quick_frame_pc_);
      // Frame sanity.
      size_t frame_size = GetCurrentQuickFrameInfo().FrameSizeInBytes();
      CHECK_NE(frame_size, 0u);
      // A rough guess at an upper size we expect to see for a frame.
      // 256 registers
      // 2 words HandleScope overhead
      // 3+3 register spills
      // TODO: this seems architecture specific for the case of JNI frames.
      // TODO: 083-compiler-regressions ManyFloatArgs shows this estimate is wrong.
      // const size_t kMaxExpectedFrameSize = (256 + 2 + 3 + 3) * sizeof(word);
      const size_t kMaxExpectedFrameSize = 2 * KB;
      //CHECK_LE(frame_size, kMaxExpectedFrameSize) << PrettyMethod(method);
      size_t return_pc_offset = GetCurrentQuickFrameInfo().GetReturnPcOffset();
      CHECK_LT(return_pc_offset, frame_size);
    }
  }
}

// Counts the number of references in the parameter list of the corresponding method.
// Note: Thus does _not_ include "this" for non-static methods.
static uint32_t GetNumberOfReferenceArgsWithoutReceiver(ArtMethod* method)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  uint32_t shorty_len;
  const char* shorty = method->GetShorty(&shorty_len);
  uint32_t refs = 0;
  for (uint32_t i = 1; i < shorty_len ; ++i) {
    if (shorty[i] == 'L') {
      refs++;
    }
  }
  return refs;
}

QuickMethodFrameInfo StackVisitor::GetCurrentQuickFrameInfo() const {
  if (cur_oat_quick_method_header_ != nullptr) {
    return cur_oat_quick_method_header_->GetFrameInfo();
  }

  ArtMethod* method = GetMethod();
  Runtime* runtime = Runtime::Current();

  if (method->IsAbstract()) {
    return runtime->GetCalleeSaveMethodFrameInfo(Runtime::kRefsAndArgs);
  }

  // This goes before IsProxyMethod since runtime methods have a null declaring class.
  if (method->IsRuntimeMethod()) {
    return runtime->GetRuntimeMethodFrameInfo(method);
  }

  if (method->IsProxyMethod()) {
    // There is only one direct method of a proxy class: the constructor. A direct method is
    // cloned from the original java.lang.reflect.Proxy and is executed as usual quick
    // compiled method without any stubs. Therefore the method must have a OatQuickMethodHeader.
    DCHECK(!method->IsDirect() && !method->IsConstructor())
        << "Constructors of proxy classes must have a OatQuickMethodHeader";
    return runtime->GetCalleeSaveMethodFrameInfo(Runtime::kRefsAndArgs);
  }

  // The only remaining case is if the method is native and uses the generic JNI stub.
  DCHECK(method->IsNative());
  ClassLinker* class_linker = runtime->GetClassLinker();
  const void* entry_point = runtime->GetInstrumentation()->GetQuickCodeFor(method, sizeof(void*));
  //DCHECK(class_linker->IsQuickGenericJniStub(entry_point)) << PrettyMethod(method);
  // Generic JNI frame.
  uint32_t handle_refs = GetNumberOfReferenceArgsWithoutReceiver(method) + 1;
  size_t scope_size = HandleScope::SizeOf(handle_refs);
  QuickMethodFrameInfo callee_info = runtime->GetCalleeSaveMethodFrameInfo(Runtime::kRefsAndArgs);

  // Callee saves + handle scope + method ref + alignment
  // Note: -sizeof(void*) since callee-save frame stores a whole method pointer.
  size_t frame_size = RoundUp(
      callee_info.FrameSizeInBytes() - sizeof(void*) + sizeof(ArtMethod*) + scope_size,
      kStackAlignment);
  return QuickMethodFrameInfo(frame_size, callee_info.CoreSpillMask(), callee_info.FpSpillMask());
}

void StackVisitor::WalkStack(bool include_transitions) {
  DCHECK(thread_ == Thread::Current() || thread_->IsSuspended());
  CHECK_EQ(cur_depth_, 0U);
  bool exit_stubs_installed = Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled();
  uint32_t instrumentation_stack_depth = 0;
  size_t inlined_frames_count = 0;

  for (const ManagedStack* current_fragment = thread_->GetManagedStack();
       current_fragment != nullptr; current_fragment = current_fragment->GetLink()) {
    cur_shadow_frame_ = current_fragment->GetTopShadowFrame();
    cur_quick_frame_ = current_fragment->GetTopQuickFrame();
    cur_quick_frame_pc_ = 0;
    cur_oat_quick_method_header_ = nullptr;

    if (cur_quick_frame_ != nullptr) {  // Handle quick stack frames.
      // Can't be both a shadow and a quick fragment.
      DCHECK(current_fragment->GetTopShadowFrame() == nullptr);
      ArtMethod* method = *cur_quick_frame_;
      while (method != nullptr) {
        cur_oat_quick_method_header_ = method->GetOatQuickMethodHeader(cur_quick_frame_pc_);
        SanityCheckFrame();

        if ((walk_kind_ == StackWalkKind::kIncludeInlinedFrames ||
             walk_kind_ == StackWalkKind::kIncludeInlinedFramesNoResolve)
            && (cur_oat_quick_method_header_ != nullptr)
            && cur_oat_quick_method_header_->IsOptimized()) {
          CodeInfo code_info = cur_oat_quick_method_header_->GetOptimizedCodeInfo();
          CodeInfoEncoding encoding = code_info.ExtractEncoding();
          uint32_t native_pc_offset =
              cur_oat_quick_method_header_->NativeQuickPcOffset(cur_quick_frame_pc_);
          StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset, encoding);
          if (stack_map.IsValid() && stack_map.HasInlineInfo(encoding.stack_map_encoding)) {
            InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map, encoding);
            DCHECK_EQ(current_inlining_depth_, 0u);
            for (current_inlining_depth_ = inline_info.GetDepth(encoding.inline_info_encoding);
                 current_inlining_depth_ != 0;
                 --current_inlining_depth_) {
              bool should_continue = VisitFrame();
              if (UNLIKELY(!should_continue)) {
                return;
              }
              cur_depth_++;
              inlined_frames_count++;
            }
          }
        }

        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }

        QuickMethodFrameInfo frame_info = GetCurrentQuickFrameInfo();
        if (context_ != nullptr) {
          context_->FillCalleeSaves(reinterpret_cast<uint8_t*>(cur_quick_frame_), frame_info);
        }
        // Compute PC for next stack frame from return PC.
        size_t frame_size = frame_info.FrameSizeInBytes();
        size_t return_pc_offset = frame_size - sizeof(void*);
        uint8_t* return_pc_addr = reinterpret_cast<uint8_t*>(cur_quick_frame_) + return_pc_offset;
        uintptr_t return_pc = *reinterpret_cast<uintptr_t*>(return_pc_addr);

        if (UNLIKELY(exit_stubs_installed)) {
          // While profiling, the return pc is restored from the side stack, except when walking
          // the stack for an exception where the side stack will be unwound in VisitFrame.
          if (reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc()) == return_pc) {
            const instrumentation::InstrumentationStackFrame& instrumentation_frame =
                GetInstrumentationStackFrame(thread_, instrumentation_stack_depth);
            instrumentation_stack_depth++;
            if (GetMethod() == Runtime::Current()->GetCalleeSaveMethod(Runtime::kSaveAll)) {
              // Skip runtime save all callee frames which are used to deliver exceptions.
            } else if (instrumentation_frame.interpreter_entry_) {
              ArtMethod* callee = Runtime::Current()->GetCalleeSaveMethod(Runtime::kRefsAndArgs);
              //CHECK_EQ(GetMethod(), callee) << "Expected: " << PrettyMethod(callee) << " Found: "
              //                              << PrettyMethod(GetMethod());
            } else {
              //CHECK_EQ(instrumentation_frame.method_, GetMethod())
              //    << "Expected: " << PrettyMethod(instrumentation_frame.method_)
              //    << " Found: " << PrettyMethod(GetMethod());
            }
            if (num_frames_ != 0) {
              // Check agreement of frame Ids only if num_frames_ is computed to avoid infinite
              // recursion.
              size_t frame_id = instrumentation::Instrumentation::ComputeFrameId(
                  thread_,
                  cur_depth_,
                  inlined_frames_count);
              CHECK_EQ(instrumentation_frame.frame_id_, frame_id);
            }
            return_pc = instrumentation_frame.return_pc_;
          }
        }

        cur_quick_frame_pc_ = return_pc;
        uint8_t* next_frame = reinterpret_cast<uint8_t*>(cur_quick_frame_) + frame_size;
        cur_quick_frame_ = reinterpret_cast<ArtMethod**>(next_frame);

        if (kDebugStackWalk) {
          //LOG(INFO) << PrettyMethod(method) << "@" << method << " size=" << frame_size
          //    << std::boolalpha
          //    << " optimized=" << (cur_oat_quick_method_header_ != nullptr &&
          //                         cur_oat_quick_method_header_->IsOptimized())
          //    << " native=" << method->IsNative()
          //    << std::noboolalpha
          //    << " entrypoints=" << method->GetEntryPointFromQuickCompiledCode()
          //    << "," << method->GetEntryPointFromJni()
          //    << " next=" << *cur_quick_frame_;
        }

        cur_depth_++;
        method = *cur_quick_frame_;
      }
    } else if (cur_shadow_frame_ != nullptr) {
      do {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }
        cur_depth_++;
        cur_shadow_frame_ = cur_shadow_frame_->GetLink();
      } while (cur_shadow_frame_ != nullptr);
    }
    if (include_transitions) {
      bool should_continue = VisitFrame();
      if (!should_continue) {
        return;
      }
    }
    cur_depth_++;
  }
  if (num_frames_ != 0) {
    CHECK_EQ(cur_depth_, num_frames_);
  }
}

void JavaFrameRootInfo::Describe(std::ostream& os) const {
  const StackVisitor* visitor = stack_visitor_;
  CHECK(visitor != nullptr);
  //os << "Type=" << GetType() << " thread_id=" << GetThreadId() << " location=" <<
  //    visitor->DescribeLocation() << " vreg=" << vreg_;
}

int StackVisitor::GetVRegOffsetFromQuickCode(const DexFile::CodeItem* code_item,
                                             uint32_t core_spills, uint32_t fp_spills,
                                             size_t frame_size, int reg, InstructionSet isa) {
  size_t pointer_size = InstructionSetPointerSize(isa);
  if (kIsDebugBuild) {
    auto* runtime = Runtime::Current();
    if (runtime != nullptr) {
      CHECK_EQ(runtime->GetClassLinker()->GetImagePointerSize(), pointer_size);
    }
  }
  DCHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK_NE(reg, -1);
  int spill_size = POPCOUNT(core_spills) * GetBytesPerGprSpillLocation(isa)
      + POPCOUNT(fp_spills) * GetBytesPerFprSpillLocation(isa)
      + sizeof(uint32_t);  // Filler.
  int num_regs = code_item->registers_size_ - code_item->ins_size_;
  int temp_threshold = code_item->registers_size_;
  const int max_num_special_temps = 1;
  if (reg == temp_threshold) {
    // The current method pointer corresponds to special location on stack.
    return 0;
  } else if (reg >= temp_threshold + max_num_special_temps) {
    /*
     * Special temporaries may have custom locations and the logic above deals with that.
     * However, non-special temporaries are placed relative to the outs.
     */
    int temps_start = code_item->outs_size_ * sizeof(uint32_t) + pointer_size /* art method */;
    int relative_offset = (reg - (temp_threshold + max_num_special_temps)) * sizeof(uint32_t);
    return temps_start + relative_offset;
  }  else if (reg < num_regs) {
    int locals_start = frame_size - spill_size - num_regs * sizeof(uint32_t);
    return locals_start + (reg * sizeof(uint32_t));
  } else {
    // Handle ins.
    return frame_size + ((reg - num_regs) * sizeof(uint32_t)) + pointer_size /* art method */;
  }
}

void LockCountData::AddMonitor(Thread* self, mirror::Object* obj) {
  if (obj == nullptr) {
    return;
  }

  // If there's an error during enter, we won't have locked the monitor. So check there's no
  // exception.
  if (self->IsExceptionPending()) {
    return;
  }

  if (monitors_ == nullptr) {
    monitors_.reset(new std::vector<mirror::Object*>());
  }
  monitors_->push_back(obj);
}

void LockCountData::RemoveMonitorOrThrow(Thread* self, const mirror::Object* obj) {
  if (obj == nullptr) {
    return;
  }
  bool found_object = false;
  if (monitors_ != nullptr) {
    // We need to remove one pointer to ref, as duplicates are used for counting recursive locks.
    // We arbitrarily choose the first one.
    auto it = std::find(monitors_->begin(), monitors_->end(), obj);
    if (it != monitors_->end()) {
      monitors_->erase(it);
      found_object = true;
    }
  }
  if (!found_object) {
    // The object wasn't found. Time for an IllegalMonitorStateException.
    // The order here isn't fully clear. Assume that any other pending exception is swallowed.
    // TODO: Maybe make already pending exception a suppressed exception.
    self->ClearException();
    self->ThrowNewExceptionF("Ljava/lang/IllegalMonitorStateException;",
                             "did not lock monitor on object of type '%s' before unlocking",
                             PrettyTypeOf(const_cast<mirror::Object*>(obj)).c_str());
  }
}

// Helper to unlock a monitor. Must be NO_THREAD_SAFETY_ANALYSIS, as we can't statically show
// that the object was locked.
void MonitorExitHelper(Thread* self, mirror::Object* obj) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  obj->MonitorExit(self);
}

bool LockCountData::CheckAllMonitorsReleasedOrThrow(Thread* self) {
  DCHECK(self != nullptr);
  if (monitors_ != nullptr) {
    if (!monitors_->empty()) {
      // There may be an exception pending, if the method is terminating abruptly. Clear it.
      // TODO: Should we add this as a suppressed exception?
      self->ClearException();

      // OK, there are monitors that are still locked. To enforce structured locking (and avoid
      // deadlocks) we unlock all of them before we raise the IllegalMonitorState exception.
      for (mirror::Object* obj : *monitors_) {
        MonitorExitHelper(self, obj);
        // If this raised an exception, ignore. TODO: Should we add this as suppressed
        // exceptions?
        if (self->IsExceptionPending()) {
          self->ClearException();
        }
      }
      // Raise an exception, just give the first object as the sample.
      mirror::Object* first = (*monitors_)[0];
      self->ThrowNewExceptionF("Ljava/lang/IllegalMonitorStateException;",
                               "did not unlock monitor on object of type '%s'",
                               PrettyTypeOf(first).c_str());

      // To make sure this path is not triggered again, clean out the monitors.
      monitors_->clear();

      return false;
    }
  }
  return true;
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
