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

#include <museum/6.0.1/art/runtime/stack.h>

#include <museum/6.0.1/art/runtime/arch/context.h>
#include <museum/6.0.1/art/runtime/art_method-inl.h>
#include <museum/6.0.1/art/runtime/base/hex_dump.h>
#include <museum/6.0.1/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/6.0.1/art/runtime/gc_map.h>
#include <museum/6.0.1/art/runtime/gc/space/image_space.h>
#include <museum/6.0.1/art/runtime/gc/space/space-inl.h>
#include <museum/6.0.1/art/runtime/linear_alloc.h>
#include <museum/6.0.1/art/runtime/mirror/class-inl.h>
#include <museum/6.0.1/art/runtime/mirror/object-inl.h>
#include <museum/6.0.1/art/runtime/mirror/object_array-inl.h>
#include <museum/6.0.1/art/runtime/quick/quick_method_frame_info.h>
#include <museum/6.0.1/art/runtime/runtime.h>
#include <museum/6.0.1/art/runtime/thread.h>
#include <museum/6.0.1/art/runtime/thread_list.h>
#include <museum/6.0.1/art/runtime/verify_object-inl.h>
#include <museum/6.0.1/art/runtime/vmap_table.h>

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
      num_frames_(num_frames),
      cur_depth_(0),
      context_(context) {
}

uint32_t StackVisitor::GetDexPc(bool abort_on_failure) const {
  if (cur_shadow_frame_ != nullptr) {
    return cur_shadow_frame_->GetDexPC();
  } else if (cur_quick_frame_ != nullptr) {
    return GetMethod()->ToDexPc(cur_quick_frame_pc_, abort_on_failure);
  } else {
    return 0;
  }
}

extern "C" mirror::Object* artQuickGetProxyThisObject(ArtMethod** sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

size_t StackVisitor::GetNativePcOffset() const {
  DCHECK(!IsShadowFrame());
  return GetMethod()->NativeQuickPcOffset(cur_quick_frame_pc_);
}

bool StackVisitor::GetRegisterIfAccessible(uint32_t reg, VRegKind kind, uint32_t* val) const {
  const bool is_float = (kind == kFloatVReg) || (kind == kDoubleLoVReg) || (kind == kDoubleHiVReg);
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
uintptr_t StackVisitor::GetReturnPc() const {
  uint8_t* sp = reinterpret_cast<uint8_t*>(GetCurrentQuickFrame());
  DCHECK(sp != nullptr);
  uint8_t* pc_addr = sp + GetMethod()->GetReturnPcOffset().SizeValue();
  return *reinterpret_cast<uintptr_t*>(pc_addr);
}

void StackVisitor::SetReturnPc(uintptr_t new_ret_pc) {
  uint8_t* sp = reinterpret_cast<uint8_t*>(GetCurrentQuickFrame());
  CHECK(sp != nullptr);
  uint8_t* pc_addr = sp + GetMethod()->GetReturnPcOffset().SizeValue();
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

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
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

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      // LOG(INFO) << "Frame Id=" << GetFrameId() << " " << DescribeLocation();
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
  result += PrettyMethod(m);
  result += StringPrintf("' at dex PC 0x%04x", GetDexPc());
  if (!IsShadowFrame()) {
    result += StringPrintf(" (native PC %p)", reinterpret_cast<void*>(GetCurrentQuickFramePc()));
  }
  return result;
}

static instrumentation::InstrumentationStackFrame& GetInstrumentationStackFrame(Thread* thread,
                                                                                uint32_t depth) {
  // CHECK_LT(depth, thread->GetInstrumentationStack()->size());
  return thread->GetInstrumentationStack()->at(depth);
}

void StackVisitor::SanityCheckFrame() const {
  if (kIsDebugBuild) {
    ArtMethod* method = GetMethod();
    auto* declaring_class = method->GetDeclaringClass();
    // Runtime methods have null declaring class.
    if (!method->IsRuntimeMethod()) {
      CHECK(declaring_class != nullptr);
      // CHECK_EQ(declaring_class->GetClass(), declaring_class->GetClass()->GetClass())
      //    << declaring_class;
    } else {
      CHECK(declaring_class == nullptr);
    }
    auto* runtime = Runtime::Current();
    auto* la = runtime->GetLinearAlloc();
    if (!la->Contains(method)) {
      // Check image space.
      bool in_image = false;
      for (auto& space : runtime->GetHeap()->GetContinuousSpaces()) {
        if (space->IsImageSpace()) {
          auto* image_space = space->AsImageSpace();
          const auto& header = image_space->GetImageHeader();
          const auto* methods = &header.GetMethodsSection();
          if (methods->Contains(reinterpret_cast<const uint8_t*>(method) - image_space->Begin())) {
            in_image = true;
            break;
          }
        }
      }
      // CHECK(in_image) << PrettyMethod(method) << " not in linear alloc or image";
    }
    if (cur_quick_frame_ != nullptr) {
      method->AssertPcIsWithinQuickCode(cur_quick_frame_pc_);
      // Frame sanity.
      size_t frame_size = method->GetFrameSizeInBytes();
      CHECK_NE(frame_size, 0u);
      // A rough guess at an upper size we expect to see for a frame.
      // 256 registers
      // 2 words HandleScope overhead
      // 3+3 register spills
      // TODO: this seems architecture specific for the case of JNI frames.
      // TODO: 083-compiler-regressions ManyFloatArgs shows this estimate is wrong.
      // const size_t kMaxExpectedFrameSize = (256 + 2 + 3 + 3) * sizeof(word);
      const size_t kMaxExpectedFrameSize = 2 * KB;
      CHECK_LE(frame_size, kMaxExpectedFrameSize);
      size_t return_pc_offset = method->GetReturnPcOffset().SizeValue();
      CHECK_LT(return_pc_offset, frame_size);
    }
  }
}

void StackVisitor::WalkStack(bool include_transitions) {
  DCHECK(thread_ == Thread::Current() || thread_->IsSuspended());
  // CHECK_EQ(cur_depth_, 0U);
  bool exit_stubs_installed = Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled();
  uint32_t instrumentation_stack_depth = 0;

  for (const ManagedStack* current_fragment = thread_->GetManagedStack();
       current_fragment != nullptr; current_fragment = current_fragment->GetLink()) {
    cur_shadow_frame_ = current_fragment->GetTopShadowFrame();
    cur_quick_frame_ = current_fragment->GetTopQuickFrame();
    cur_quick_frame_pc_ = 0;

    if (cur_quick_frame_ != nullptr) {  // Handle quick stack frames.
      // Can't be both a shadow and a quick fragment.
      DCHECK(current_fragment->GetTopShadowFrame() == nullptr);
      ArtMethod* method = *cur_quick_frame_;
      while (method != nullptr) {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }

        if (context_ != nullptr) {
          context_->FillCalleeSaves(*this);
        }
        size_t frame_size = method->GetFrameSizeInBytes();
        // Compute PC for next stack frame from return PC.
        size_t return_pc_offset = method->GetReturnPcOffset(frame_size).SizeValue();
        uint8_t* return_pc_addr = reinterpret_cast<uint8_t*>(cur_quick_frame_) + return_pc_offset;
        uintptr_t return_pc = *reinterpret_cast<uintptr_t*>(return_pc_addr);
        if (UNLIKELY(exit_stubs_installed)) {
          return;
        }
        cur_quick_frame_pc_ = return_pc;
        uint8_t* next_frame = reinterpret_cast<uint8_t*>(cur_quick_frame_) + frame_size;
        cur_quick_frame_ = reinterpret_cast<ArtMethod**>(next_frame);

        if (kDebugStackWalk) {
          // LOG(INFO) << PrettyMethod(method) << "@" << method << " size=" << frame_size
          //     << " optimized=" << method->IsOptimized(sizeof(void*))
          //     << " native=" << method->IsNative()
          //     << " entrypoints=" << method->GetEntryPointFromQuickCompiledCode()
          //     << "," << method->GetEntryPointFromJni()
          //     << "," << method->GetEntryPointFromInterpreter()
          //     << " next=" << *cur_quick_frame_;
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
    // CHECK_EQ(cur_depth_, num_frames_);
  }
}

void JavaFrameRootInfo::Describe(std::ostream& os) const {
  const StackVisitor* visitor = stack_visitor_;
  CHECK(visitor != nullptr);
  // os << "Type=" << GetType() << " thread_id=" << GetThreadId() << " location=" <<
  //     visitor->DescribeLocation() << " vreg=" << vreg_;
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
  DCHECK_EQ(frame_size & (kStackAlignment - 1), 0U);
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

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
