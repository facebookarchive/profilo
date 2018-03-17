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

#include <museum/5.1.1/art/runtime/stack.h>

#include <museum/5.1.1/art/runtime/arch/context.h>
#include <museum/5.1.1/art/runtime/base/hex_dump.h>
#include <museum/5.1.1/art/runtime/mirror/art_method-inl.h>
#include <museum/5.1.1/art/runtime/mirror/class-inl.h>
#include <museum/5.1.1/art/runtime/mirror/object.h>
#include <museum/5.1.1/art/runtime/mirror/object-inl.h>
#include <museum/5.1.1/art/runtime/mirror/object_array-inl.h>
#include <museum/5.1.1/art/runtime/quick/quick_method_frame_info.h>
#include <museum/5.1.1/art/runtime/runtime.h>
#include <museum/5.1.1/art/runtime/thread.h>
#include <museum/5.1.1/art/runtime/thread_list.h>
#include <museum/5.1.1/art/runtime/throw_location.h>
#include <museum/5.1.1/art/runtime/verify_object-inl.h>
#include <museum/5.1.1/art/runtime/vmap_table.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

mirror::Object* ShadowFrame::GetThisObject() const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else if (m->IsNative()) {
    return GetVRegReference(0);
  } else {
    const DexFile::CodeItem* code_item = m->GetCodeItem();
    uint16_t reg = code_item->registers_size_ - code_item->ins_size_;
    return GetVRegReference(reg);
  }
}

mirror::Object* ShadowFrame::GetThisObject(uint16_t num_ins) const {
  mirror::ArtMethod* m = GetMethod();
  if (m->IsStatic()) {
    return NULL;
  } else {
    return GetVRegReference(NumberOfVRegs() - num_ins);
  }
}

ThrowLocation ShadowFrame::GetCurrentLocationForThrow() const {
  return ThrowLocation(GetThisObject(), GetMethod(), GetDexPC());
}

size_t ManagedStack::NumJniShadowFrameReferences() const {
  size_t count = 0;
  for (const ManagedStack* current_fragment = this; current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != NULL;
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
  for (const ManagedStack* current_fragment = this; current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    for (ShadowFrame* current_frame = current_fragment->top_shadow_frame_; current_frame != NULL;
         current_frame = current_frame->GetLink()) {
      if (current_frame->Contains(shadow_frame_entry)) {
        return true;
      }
    }
  }
  return false;
}

StackVisitor::StackVisitor(Thread* thread, Context* context)
    : thread_(thread), cur_shadow_frame_(NULL),
      cur_quick_frame_(NULL), cur_quick_frame_pc_(0), num_frames_(0), cur_depth_(0),
      context_(context) {
  DCHECK(thread == Thread::Current() || thread->IsSuspended()) << *thread;
}

StackVisitor::StackVisitor(Thread* thread, Context* context, size_t num_frames)
    : thread_(thread), cur_shadow_frame_(NULL),
      cur_quick_frame_(NULL), cur_quick_frame_pc_(0), num_frames_(num_frames), cur_depth_(0),
      context_(context) {
  DCHECK(thread == Thread::Current() || thread->IsSuspended()) << *thread;
}

uint32_t StackVisitor::GetDexPc(bool abort_on_failure) const {
  if (cur_shadow_frame_ != NULL) {
    return cur_shadow_frame_->GetDexPC();
  } else if (cur_quick_frame_ != NULL) {
    return GetMethod()->ToDexPc(cur_quick_frame_pc_, abort_on_failure);
  } else {
    return 0;
  }
}

extern "C" mirror::Object* artQuickGetProxyThisObject(StackReference<mirror::ArtMethod>* sp)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

size_t StackVisitor::GetNativePcOffset() const {
  DCHECK(!IsShadowFrame());
  return GetMethod()->NativePcOffset(cur_quick_frame_pc_);
}

uintptr_t* StackVisitor::GetGPRAddress(uint32_t reg) const {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  return context_->GetGPRAddress(reg);
}

bool StackVisitor::GetGPR(uint32_t reg, uintptr_t* val) const {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  return context_->GetGPR(reg, val);
}

bool StackVisitor::SetGPR(uint32_t reg, uintptr_t value) {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  return context_->SetGPR(reg, value);
}

bool StackVisitor::GetFPR(uint32_t reg, uintptr_t* val) const {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  return context_->GetFPR(reg, val);
}

bool StackVisitor::SetFPR(uint32_t reg, uintptr_t value) {
  DCHECK(cur_quick_frame_ != NULL) << "This is a quick frame routine";
  return context_->SetFPR(reg, value);
}

uintptr_t StackVisitor::GetReturnPc() const {
  byte* sp = reinterpret_cast<byte*>(GetCurrentQuickFrame());
  DCHECK(sp != NULL);
  byte* pc_addr = sp + GetMethod()->GetReturnPcOffsetInBytes();
  return *reinterpret_cast<uintptr_t*>(pc_addr);
}

void StackVisitor::SetReturnPc(uintptr_t new_ret_pc) {
  byte* sp = reinterpret_cast<byte*>(GetCurrentQuickFrame());
  CHECK(sp != NULL);
  byte* pc_addr = sp + GetMethod()->GetReturnPcOffsetInBytes();
  *reinterpret_cast<uintptr_t*>(pc_addr) = new_ret_pc;
}

size_t StackVisitor::ComputeNumFrames(Thread* thread) {
  struct NumFramesVisitor : public StackVisitor {
    explicit NumFramesVisitor(Thread* thread)
        : StackVisitor(thread, NULL), frames(0) {}

    bool VisitFrame() OVERRIDE {
      frames++;
      return true;
    }

    size_t frames;
  };
  NumFramesVisitor visitor(thread);
  visitor.WalkStack(true);
  return visitor.frames;
}

bool StackVisitor::GetNextMethodAndDexPc(mirror::ArtMethod** next_method, uint32_t* next_dex_pc) {
  struct HasMoreFramesVisitor : public StackVisitor {
    explicit HasMoreFramesVisitor(Thread* thread, size_t num_frames, size_t frame_height)
        : StackVisitor(thread, nullptr, num_frames), frame_height_(frame_height),
          found_frame_(false), has_more_frames_(false), next_method_(nullptr), next_dex_pc_(0) {
    }

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      if (found_frame_) {
        mirror::ArtMethod* method = GetMethod();
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
    mirror::ArtMethod* next_method_;
    uint32_t next_dex_pc_;
  };
  HasMoreFramesVisitor visitor(thread_, GetNumFrames(), GetFrameHeight());
  visitor.WalkStack(true);
  *next_method = visitor.next_method_;
  *next_dex_pc = visitor.next_dex_pc_;
  return visitor.has_more_frames_;
}

void StackVisitor::DescribeStack(Thread* thread) {
  struct DescribeStackVisitor : public StackVisitor {
    explicit DescribeStackVisitor(Thread* thread)
        : StackVisitor(thread, NULL) {}

    bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      LOG(INFO) << "Frame Id=" << GetFrameId() << " " << DescribeLocation();
      return true;
    }
  };
  DescribeStackVisitor visitor(thread);
  visitor.WalkStack(true);
}

std::string StackVisitor::DescribeLocation() const {
  std::string result("Visiting method '");
  mirror::ArtMethod* m = GetMethod();
  if (m == NULL) {
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
  CHECK_LT(depth, thread->GetInstrumentationStack()->size());
  return thread->GetInstrumentationStack()->at(depth);
}

void StackVisitor::SanityCheckFrame() const {
  if (kIsDebugBuild) {
    mirror::ArtMethod* method = GetMethod();
    // CHECK_EQ(method->GetClass(), mirror::ArtMethod::GetJavaLangReflectArtMethod());
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
      size_t return_pc_offset = method->GetReturnPcOffsetInBytes();
      CHECK_LT(return_pc_offset, frame_size);
    }
  }
}

void StackVisitor::WalkStack(bool include_transitions) {
  // CHECK_EQ(cur_depth_, 0U);
  // FB Enforcing false as getting the opposite at runtime
  bool exit_stubs_installed = false; // Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled();
  uint32_t instrumentation_stack_depth = 0;

  for (const ManagedStack* current_fragment = thread_->GetManagedStack(); current_fragment != NULL;
       current_fragment = current_fragment->GetLink()) {
    cur_shadow_frame_ = current_fragment->GetTopShadowFrame();
    cur_quick_frame_ = current_fragment->GetTopQuickFrame();
    cur_quick_frame_pc_ = current_fragment->GetTopQuickFramePc();

    if (cur_quick_frame_ != NULL) {  // Handle quick stack frames.
      // Can't be both a shadow and a quick fragment.
      DCHECK(current_fragment->GetTopShadowFrame() == NULL);
      mirror::ArtMethod* method = cur_quick_frame_->AsMirrorPtr();
      while (method != NULL) {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }

        if (context_ != NULL) {
          context_->FillCalleeSaves(*this);
        }
        size_t frame_size = method->GetFrameSizeInBytes();
        // Compute PC for next stack frame from return PC.
        size_t return_pc_offset = method->GetReturnPcOffsetInBytes(frame_size);
        byte* return_pc_addr = reinterpret_cast<byte*>(cur_quick_frame_) + return_pc_offset;
        uintptr_t return_pc = *reinterpret_cast<uintptr_t*>(return_pc_addr);
        if (UNLIKELY(exit_stubs_installed)) {
          return;
        }
        cur_quick_frame_pc_ = return_pc;
        byte* next_frame = reinterpret_cast<byte*>(cur_quick_frame_) + frame_size;
        cur_quick_frame_ = reinterpret_cast<StackReference<mirror::ArtMethod>*>(next_frame);
        cur_depth_++;
        method = cur_quick_frame_->AsMirrorPtr();
      }
    } else if (cur_shadow_frame_ != NULL) {
      do {
        SanityCheckFrame();
        bool should_continue = VisitFrame();
        if (UNLIKELY(!should_continue)) {
          return;
        }
        cur_depth_++;
        cur_shadow_frame_ = cur_shadow_frame_->GetLink();
      } while (cur_shadow_frame_ != NULL);
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
  //    visitor->DescribeLocation() << " vreg=" << vreg_;
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
