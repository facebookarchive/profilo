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

#ifndef ART_RUNTIME_INTERPRETER_SHADOW_FRAME_H_
#define ART_RUNTIME_INTERPRETER_SHADOW_FRAME_H_

#include <cstring>
#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "lock_count_data.h"
#include "read_barrier.h"
#include "stack_reference.h"
#include "verify_object.h"

namespace art {

namespace mirror {
  class Object;
}  // namespace mirror

class ArtMethod;
class ShadowFrame;
class Thread;
union JValue;

// Forward declaration. Just calls the destructor.
struct ShadowFrameDeleter;
using ShadowFrameAllocaUniquePtr = std::unique_ptr<ShadowFrame, ShadowFrameDeleter>;

// ShadowFrame has 2 possible layouts:
//  - interpreter - separate VRegs and reference arrays. References are in the reference array.
//  - JNI - just VRegs, but where every VReg holds a reference.
class ShadowFrame {
 public:
  // Compute size of ShadowFrame in bytes assuming it has a reference array.
  static size_t ComputeSize(uint32_t num_vregs) {
    return sizeof(ShadowFrame) + (sizeof(uint32_t) * num_vregs) +
           (sizeof(StackReference<mirror::Object>) * num_vregs);
  }

  // Create ShadowFrame in heap for deoptimization.
  static ShadowFrame* CreateDeoptimizedFrame(uint32_t num_vregs, ShadowFrame* link,
                                             ArtMethod* method, uint32_t dex_pc) {
    uint8_t* memory = new uint8_t[ComputeSize(num_vregs)];
    return CreateShadowFrameImpl(num_vregs, link, method, dex_pc, memory);
  }

  // Delete a ShadowFrame allocated on the heap for deoptimization.
  static void DeleteDeoptimizedFrame(ShadowFrame* sf) {
    sf->~ShadowFrame();  // Explicitly destruct.
    uint8_t* memory = reinterpret_cast<uint8_t*>(sf);
    delete[] memory;
  }

  // Create a shadow frame in a fresh alloca. This needs to be in the context of the caller.
  // Inlining doesn't work, the compiler will still undo the alloca. So this needs to be a macro.
#define CREATE_SHADOW_FRAME(num_vregs, link, method, dex_pc) ({                              \
    size_t frame_size = ShadowFrame::ComputeSize(num_vregs);                                 \
    void* alloca_mem = alloca(frame_size);                                                   \
    ShadowFrameAllocaUniquePtr(                                                              \
        ShadowFrame::CreateShadowFrameImpl((num_vregs), (link), (method), (dex_pc),          \
                                           (alloca_mem)));                                   \
    })

  ~ShadowFrame() {}

  // TODO(iam): Clean references array up since they're always there,
  // we don't need to do conditionals.
  bool HasReferenceArray() const {
    return true;
  }

  uint32_t NumberOfVRegs() const {
    return number_of_vregs_;
  }

  uint32_t GetDexPC() const {
    return (dex_pc_ptr_ == nullptr) ? dex_pc_ : dex_pc_ptr_ - code_item_->insns_;
  }

  int16_t GetCachedHotnessCountdown() const {
    return cached_hotness_countdown_;
  }

  void SetCachedHotnessCountdown(int16_t cached_hotness_countdown) {
    cached_hotness_countdown_ = cached_hotness_countdown;
  }

  int16_t GetHotnessCountdown() const {
    return hotness_countdown_;
  }

  void SetHotnessCountdown(int16_t hotness_countdown) {
    hotness_countdown_ = hotness_countdown;
  }

  void SetDexPC(uint32_t dex_pc) {
    dex_pc_ = dex_pc;
    dex_pc_ptr_ = nullptr;
  }

  ShadowFrame* GetLink() const {
    return link_;
  }

  void SetLink(ShadowFrame* frame) {
    DCHECK_NE(this, frame);
    link_ = frame;
  }

  int32_t GetVReg(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const int32_t*>(vreg);
  }

  // Shorts are extended to Ints in VRegs.  Interpreter intrinsics needs them as shorts.
  int16_t GetVRegShort(size_t i) const {
    return static_cast<int16_t>(GetVReg(i));
  }

  uint32_t* GetVRegAddr(size_t i) {
    return &vregs_[i];
  }

  uint32_t* GetShadowRefAddr(size_t i) {
    DCHECK(HasReferenceArray());
    DCHECK_LT(i, NumberOfVRegs());
    return &vregs_[i + NumberOfVRegs()];
  }

  void SetCodeItem(const DexFile::CodeItem* code_item) {
    code_item_ = code_item;
  }

  const DexFile::CodeItem* GetCodeItem() const {
    return code_item_;
  }

  float GetVRegFloat(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    // NOTE: Strict-aliasing?
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const float*>(vreg);
  }

  int64_t GetVRegLong(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    typedef const int64_t unaligned_int64 __attribute__ ((aligned (4)));
    return *reinterpret_cast<unaligned_int64*>(vreg);
  }

  double GetVRegDouble(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    typedef const double unaligned_double __attribute__ ((aligned (4)));
    return *reinterpret_cast<unaligned_double*>(vreg);
  }

  // Look up the reference given its virtual register number.
  // If this returns non-null then this does not mean the vreg is currently a reference
  // on non-moving collectors. Check that the raw reg with GetVReg is equal to this if not certain.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  mirror::Object* GetVRegReference(size_t i) const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_LT(i, NumberOfVRegs());
    mirror::Object* ref;
    if (HasReferenceArray()) {
      ref = References()[i].AsMirrorPtr();
    } else {
      const uint32_t* vreg_ptr = &vregs_[i];
      ref = reinterpret_cast<const StackReference<mirror::Object>*>(vreg_ptr)->AsMirrorPtr();
    }
    if (kUseReadBarrier) {
      ReadBarrier::AssertToSpaceInvariant(ref);
    }
    if (kVerifyFlags & kVerifyReads) {
      VerifyObject(ref);
    }
    return ref;
  }

  // Get view of vregs as range of consecutive arguments starting at i.
  uint32_t* GetVRegArgs(size_t i) {
    return &vregs_[i];
  }

  void SetVReg(size_t i, int32_t val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<int32_t*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
    }
  }

  void SetVRegFloat(size_t i, float val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    *reinterpret_cast<float*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
    }
  }

  void SetVRegLong(size_t i, int64_t val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    typedef int64_t unaligned_int64 __attribute__ ((aligned (4)));
    *reinterpret_cast<unaligned_int64*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
      References()[i + 1].Clear();
    }
  }

  void SetVRegDouble(size_t i, double val) {
    DCHECK_LT(i, NumberOfVRegs());
    uint32_t* vreg = &vregs_[i];
    typedef double unaligned_double __attribute__ ((aligned (4)));
    *reinterpret_cast<unaligned_double*>(vreg) = val;
    // This is needed for moving collectors since these can update the vreg references if they
    // happen to agree with references in the reference array.
    if (kMovingCollector && HasReferenceArray()) {
      References()[i].Clear();
      References()[i + 1].Clear();
    }
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetVRegReference(size_t i, mirror::Object* val) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_LT(i, NumberOfVRegs());
    if (kVerifyFlags & kVerifyWrites) {
      VerifyObject(val);
    }
    if (kUseReadBarrier) {
      ReadBarrier::AssertToSpaceInvariant(val);
    }
    uint32_t* vreg = &vregs_[i];
    reinterpret_cast<StackReference<mirror::Object>*>(vreg)->Assign(val);
    if (HasReferenceArray()) {
      References()[i].Assign(val);
    }
  }

  void SetMethod(ArtMethod* method) REQUIRES(Locks::mutator_lock_) {
    DCHECK(method != nullptr);
    DCHECK(method_ != nullptr);
    method_ = method;
  }

  ArtMethod* GetMethod() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(method_ != nullptr);
    return method_;
  }

  mirror::Object* GetThisObject() const REQUIRES_SHARED(Locks::mutator_lock_);

  mirror::Object* GetThisObject(uint16_t num_ins) const REQUIRES_SHARED(Locks::mutator_lock_);

  bool Contains(StackReference<mirror::Object>* shadow_frame_entry_obj) const {
    if (HasReferenceArray()) {
      return ((&References()[0] <= shadow_frame_entry_obj) &&
              (shadow_frame_entry_obj <= (&References()[NumberOfVRegs() - 1])));
    } else {
      uint32_t* shadow_frame_entry = reinterpret_cast<uint32_t*>(shadow_frame_entry_obj);
      return ((&vregs_[0] <= shadow_frame_entry) &&
              (shadow_frame_entry <= (&vregs_[NumberOfVRegs() - 1])));
    }
  }

  LockCountData& GetLockCountData() {
    return lock_count_data_;
  }

  static size_t LockCountDataOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, lock_count_data_);
  }

  static size_t LinkOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, link_);
  }

  static size_t MethodOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, method_);
  }

  static size_t DexPCOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_pc_);
  }

  static size_t NumberOfVRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, number_of_vregs_);
  }

  static size_t VRegsOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, vregs_);
  }

  static size_t ResultRegisterOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, result_register_);
  }

  static size_t DexPCPtrOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, dex_pc_ptr_);
  }

  static size_t CodeItemOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, code_item_);
  }

  static size_t CachedHotnessCountdownOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, cached_hotness_countdown_);
  }

  static size_t HotnessCountdownOffset() {
    return OFFSETOF_MEMBER(ShadowFrame, hotness_countdown_);
  }

  // Create ShadowFrame for interpreter using provided memory.
  static ShadowFrame* CreateShadowFrameImpl(uint32_t num_vregs,
                                            ShadowFrame* link,
                                            ArtMethod* method,
                                            uint32_t dex_pc,
                                            void* memory) {
    return new (memory) ShadowFrame(num_vregs, link, method, dex_pc, true);
  }

  const uint16_t* GetDexPCPtr() {
    return dex_pc_ptr_;
  }

  void SetDexPCPtr(uint16_t* dex_pc_ptr) {
    dex_pc_ptr_ = dex_pc_ptr;
  }

  JValue* GetResultRegister() {
    return result_register_;
  }

 private:
  ShadowFrame(uint32_t num_vregs, ShadowFrame* link, ArtMethod* method,
              uint32_t dex_pc, bool has_reference_array)
      : link_(link),
        method_(method),
        result_register_(nullptr),
        dex_pc_ptr_(nullptr),
        code_item_(nullptr),
        number_of_vregs_(num_vregs),
        dex_pc_(dex_pc),
        cached_hotness_countdown_(0),
        hotness_countdown_(0) {
    // TODO(iam): Remove this parameter, it's an an artifact of portable removal
    DCHECK(has_reference_array);
    if (has_reference_array) {
      memset(vregs_, 0, num_vregs * (sizeof(uint32_t) + sizeof(StackReference<mirror::Object>)));
    } else {
      memset(vregs_, 0, num_vregs * sizeof(uint32_t));
    }
  }

  const StackReference<mirror::Object>* References() const {
    DCHECK(HasReferenceArray());
    const uint32_t* vreg_end = &vregs_[NumberOfVRegs()];
    return reinterpret_cast<const StackReference<mirror::Object>*>(vreg_end);
  }

  StackReference<mirror::Object>* References() {
    return const_cast<StackReference<mirror::Object>*>(
        const_cast<const ShadowFrame*>(this)->References());
  }

  // Link to previous shadow frame or null.
  ShadowFrame* link_;
  ArtMethod* method_;
  JValue* result_register_;
  const uint16_t* dex_pc_ptr_;
  const DexFile::CodeItem* code_item_;
  LockCountData lock_count_data_;  // This may contain GC roots when lock counting is active.
  const uint32_t number_of_vregs_;
  uint32_t dex_pc_;
  int16_t cached_hotness_countdown_;
  int16_t hotness_countdown_;

  // This is a two-part array:
  //  - [0..number_of_vregs) holds the raw virtual registers, and each element here is always 4
  //    bytes.
  //  - [number_of_vregs..number_of_vregs*2) holds only reference registers. Each element here is
  //    ptr-sized.
  // In other words when a primitive is stored in vX, the second (reference) part of the array will
  // be null. When a reference is stored in vX, the second (reference) part of the array will be a
  // copy of vX.
  uint32_t vregs_[0];

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShadowFrame);
};

struct ShadowFrameDeleter {
  inline void operator()(ShadowFrame* frame) {
    if (frame != nullptr) {
      frame->~ShadowFrame();
    }
  }
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_SHADOW_FRAME_H_
