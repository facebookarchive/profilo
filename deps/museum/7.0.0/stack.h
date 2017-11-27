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

#ifndef ART_RUNTIME_STACK_H_
#define ART_RUNTIME_STACK_H_

#include <stdint.h>
#include <string>

#include "arch/instruction_set.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include "gc_root.h"
#include "mirror/object_reference.h"
#include "quick/quick_method_frame_info.h"
#include "read_barrier.h"
#include "verify_object.h"

namespace art {

namespace mirror {
  class Object;
}  // namespace mirror

class ArtMethod;
class Context;
class HandleScope;
class InlineInfo;
class OatQuickMethodHeader;
class ScopedObjectAccess;
class ShadowFrame;
class StackVisitor;
class Thread;

// The kind of vreg being accessed in calls to Set/GetVReg.
enum VRegKind {
  kReferenceVReg,
  kIntVReg,
  kFloatVReg,
  kLongLoVReg,
  kLongHiVReg,
  kDoubleLoVReg,
  kDoubleHiVReg,
  kConstant,
  kImpreciseConstant,
  kUndefined,
};
std::ostream& operator<<(std::ostream& os, const VRegKind& rhs);

// A reference from the shadow stack to a MirrorType object within the Java heap.
template<class MirrorType>
class MANAGED StackReference : public mirror::CompressedReference<MirrorType> {
};

// Forward declaration. Just calls the destructor.
struct ShadowFrameDeleter;
using ShadowFrameAllocaUniquePtr = std::unique_ptr<ShadowFrame, ShadowFrameDeleter>;

// Counting locks by storing object pointers into a vector. Duplicate entries mark recursive locks.
// The vector will be visited with the ShadowFrame during GC (so all the locked-on objects are
// thread roots).
// Note: implementation is split so that the call sites may be optimized to no-ops in case no
//       lock counting is necessary. The actual implementation is in the cc file to avoid
//       dependencies.
class LockCountData {
 public:
  // Add the given object to the list of monitors, that is, objects that have been locked. This
  // will not throw (but be skipped if there is an exception pending on entry).
  void AddMonitor(Thread* self, mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_);

  // Try to remove the given object from the monitor list, indicating an unlock operation.
  // This will throw an IllegalMonitorStateException (clearing any already pending exception), in
  // case that there wasn't a lock recorded for the object.
  void RemoveMonitorOrThrow(Thread* self,
                            const mirror::Object* obj) SHARED_REQUIRES(Locks::mutator_lock_);

  // Check whether all acquired monitors have been released. This will potentially throw an
  // IllegalMonitorStateException, clearing any already pending exception. Returns true if the
  // check shows that everything is OK wrt/ lock counting, false otherwise.
  bool CheckAllMonitorsReleasedOrThrow(Thread* self) SHARED_REQUIRES(Locks::mutator_lock_);

  template <typename T, typename... Args>
  void VisitMonitors(T visitor, Args&&... args) SHARED_REQUIRES(Locks::mutator_lock_) {
    if (monitors_ != nullptr) {
      // Visitors may change the Object*. Be careful with the foreach loop.
      for (mirror::Object*& obj : *monitors_) {
        visitor(/* inout */ &obj, std::forward<Args>(args)...);
      }
    }
  }

 private:
  // Stores references to the locked-on objects. As noted, this should be visited during thread
  // marking.
  std::unique_ptr<std::vector<mirror::Object*>> monitors_;
};

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

  float GetVRegFloat(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    // NOTE: Strict-aliasing?
    const uint32_t* vreg = &vregs_[i];
    return *reinterpret_cast<const float*>(vreg);
  }

  int64_t GetVRegLong(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    // Alignment attribute required for GCC 4.8
    typedef const int64_t unaligned_int64 __attribute__ ((aligned (4)));
    return *reinterpret_cast<unaligned_int64*>(vreg);
  }

  double GetVRegDouble(size_t i) const {
    DCHECK_LT(i, NumberOfVRegs());
    const uint32_t* vreg = &vregs_[i];
    // Alignment attribute required for GCC 4.8
    typedef const double unaligned_double __attribute__ ((aligned (4)));
    return *reinterpret_cast<unaligned_double*>(vreg);
  }

  // Look up the reference given its virtual register number.
  // If this returns non-null then this does not mean the vreg is currently a reference
  // on non-moving collectors. Check that the raw reg with GetVReg is equal to this if not certain.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  mirror::Object* GetVRegReference(size_t i) const SHARED_REQUIRES(Locks::mutator_lock_) {
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
    // Alignment attribute required for GCC 4.8
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
    // Alignment attribute required for GCC 4.8
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
  void SetVRegReference(size_t i, mirror::Object* val) SHARED_REQUIRES(Locks::mutator_lock_) {
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

  ArtMethod* GetMethod() const SHARED_REQUIRES(Locks::mutator_lock_) {
    DCHECK(method_ != nullptr);
    return method_;
  }

  mirror::Object* GetThisObject() const SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::Object* GetThisObject(uint16_t num_ins) const SHARED_REQUIRES(Locks::mutator_lock_);

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

  JValue* GetResultRegister() {
    return result_register_;
  }

 private:
  ShadowFrame(uint32_t num_vregs, ShadowFrame* link, ArtMethod* method,
              uint32_t dex_pc, bool has_reference_array)
      : link_(link), method_(method), result_register_(nullptr), dex_pc_ptr_(nullptr),
        code_item_(nullptr), number_of_vregs_(num_vregs), dex_pc_(dex_pc) {
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

class JavaFrameRootInfo : public RootInfo {
 public:
  JavaFrameRootInfo(uint32_t thread_id, const StackVisitor* stack_visitor, size_t vreg)
     : RootInfo(kRootJavaFrame, thread_id), stack_visitor_(stack_visitor), vreg_(vreg) {
  }
  virtual void Describe(std::ostream& os) const OVERRIDE
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  const StackVisitor* const stack_visitor_;
  const size_t vreg_;
};

// The managed stack is used to record fragments of managed code stacks. Managed code stacks
// may either be shadow frames or lists of frames using fixed frame sizes. Transition records are
// necessary for transitions between code using different frame layouts and transitions into native
// code.
class PACKED(4) ManagedStack {
 public:
  ManagedStack()
      : top_quick_frame_(nullptr), link_(nullptr), top_shadow_frame_(nullptr) {}

  void PushManagedStackFragment(ManagedStack* fragment) {
    // Copy this top fragment into given fragment.
    memcpy(fragment, this, sizeof(ManagedStack));
    // Clear this fragment, which has become the top.
    memset(this, 0, sizeof(ManagedStack));
    // Link our top fragment onto the given fragment.
    link_ = fragment;
  }

  void PopManagedStackFragment(const ManagedStack& fragment) {
    DCHECK(&fragment == link_);
    // Copy this given fragment back to the top.
    memcpy(this, &fragment, sizeof(ManagedStack));
  }

  ManagedStack* GetLink() const {
    return link_;
  }

  ArtMethod** GetTopQuickFrame() const {
    return top_quick_frame_;
  }

  void SetTopQuickFrame(ArtMethod** top) {
    DCHECK(top_shadow_frame_ == nullptr);
    top_quick_frame_ = top;
  }

  static size_t TopQuickFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_quick_frame_);
  }

  ShadowFrame* PushShadowFrame(ShadowFrame* new_top_frame) {
    DCHECK(top_quick_frame_ == nullptr);
    ShadowFrame* old_frame = top_shadow_frame_;
    top_shadow_frame_ = new_top_frame;
    new_top_frame->SetLink(old_frame);
    return old_frame;
  }

  ShadowFrame* PopShadowFrame() {
    DCHECK(top_quick_frame_ == nullptr);
    CHECK(top_shadow_frame_ != nullptr);
    ShadowFrame* frame = top_shadow_frame_;
    top_shadow_frame_ = frame->GetLink();
    return frame;
  }

  ShadowFrame* GetTopShadowFrame() const {
    return top_shadow_frame_;
  }

  void SetTopShadowFrame(ShadowFrame* top) {
    DCHECK(top_quick_frame_ == nullptr);
    top_shadow_frame_ = top;
  }

  static size_t TopShadowFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_shadow_frame_);
  }

  size_t NumJniShadowFrameReferences() const SHARED_REQUIRES(Locks::mutator_lock_);

  bool ShadowFramesContain(StackReference<mirror::Object>* shadow_frame_entry) const;

 private:
  ArtMethod** top_quick_frame_;
  ManagedStack* link_;
  ShadowFrame* top_shadow_frame_;
};

class StackVisitor {
 public:
  // This enum defines a flag to control whether inlined frames are included
  // when walking the stack.
  enum class StackWalkKind {
    kIncludeInlinedFrames,
    kIncludeInlinedFramesNoResolve,
    kSkipInlinedFrames,
  };

 protected:
  StackVisitor(Thread* thread, Context* context, StackWalkKind walk_kind)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool GetRegisterIfAccessible(uint32_t reg, VRegKind kind, uint32_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);

 public:
  virtual ~StackVisitor() {}

  // Return 'true' if we should continue to visit more frames, 'false' to stop.
  virtual bool VisitFrame() SHARED_REQUIRES(Locks::mutator_lock_) = 0;

  void WalkStack(bool include_transitions = false)
      SHARED_REQUIRES(Locks::mutator_lock_);

  Thread* GetThread() const {
    return thread_;
  }

  ArtMethod* GetMethod() const SHARED_REQUIRES(Locks::mutator_lock_);

  ArtMethod* GetOuterMethod() const {
    return *GetCurrentQuickFrame();
  }

  bool IsShadowFrame() const {
    return cur_shadow_frame_ != nullptr;
  }

  uint32_t GetDexPc(bool abort_on_failure = true) const SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::Object* GetThisObject() const SHARED_REQUIRES(Locks::mutator_lock_);

  size_t GetNativePcOffset() const SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns the height of the stack in the managed stack frames, including transitions.
  size_t GetFrameHeight() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetNumFrames() - cur_depth_ - 1;
  }

  // Returns a frame ID for JDWP use, starting from 1.
  size_t GetFrameId() SHARED_REQUIRES(Locks::mutator_lock_) {
    return GetFrameHeight() + 1;
  }

  size_t GetNumFrames() SHARED_REQUIRES(Locks::mutator_lock_) {
    if (num_frames_ == 0) {
      num_frames_ = ComputeNumFrames(thread_, walk_kind_);
    }
    return num_frames_;
  }

  size_t GetFrameDepth() SHARED_REQUIRES(Locks::mutator_lock_) {
    return cur_depth_;
  }

  // Get the method and dex pc immediately after the one that's currently being visited.
  bool GetNextMethodAndDexPc(ArtMethod** next_method, uint32_t* next_dex_pc)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool GetVReg(ArtMethod* m, uint16_t vreg, VRegKind kind, uint32_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool GetVRegPair(ArtMethod* m, uint16_t vreg, VRegKind kind_lo, VRegKind kind_hi,
                   uint64_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Values will be set in debugger shadow frames. Debugger will make sure deoptimization
  // is triggered to make the values effective.
  bool SetVReg(ArtMethod* m, uint16_t vreg, uint32_t new_value, VRegKind kind)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Values will be set in debugger shadow frames. Debugger will make sure deoptimization
  // is triggered to make the values effective.
  bool SetVRegPair(ArtMethod* m,
                   uint16_t vreg,
                   uint64_t new_value,
                   VRegKind kind_lo,
                   VRegKind kind_hi)
      SHARED_REQUIRES(Locks::mutator_lock_);

  uintptr_t* GetGPRAddress(uint32_t reg) const;

  // This is a fast-path for getting/setting values in a quick frame.
  uint32_t* GetVRegAddrFromQuickCode(ArtMethod** cur_quick_frame,
                                     const DexFile::CodeItem* code_item,
                                     uint32_t core_spills, uint32_t fp_spills, size_t frame_size,
                                     uint16_t vreg) const {
    int offset = GetVRegOffsetFromQuickCode(
        code_item, core_spills, fp_spills, frame_size, vreg, kRuntimeISA);
    DCHECK_EQ(cur_quick_frame, GetCurrentQuickFrame());
    uint8_t* vreg_addr = reinterpret_cast<uint8_t*>(cur_quick_frame) + offset;
    return reinterpret_cast<uint32_t*>(vreg_addr);
  }

  uintptr_t GetReturnPc() const SHARED_REQUIRES(Locks::mutator_lock_);

  void SetReturnPc(uintptr_t new_ret_pc) SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * Return sp-relative offset for a Dalvik virtual register, compiler
   * spill or Method* in bytes using Method*.
   * Note that (reg == -1) denotes an invalid Dalvik register. For the
   * positive values, the Dalvik registers come first, followed by the
   * Method*, followed by other special temporaries if any, followed by
   * regular compiler temporary. As of now we only have the Method* as
   * as a special compiler temporary.
   * A compiler temporary can be thought of as a virtual register that
   * does not exist in the dex but holds intermediate values to help
   * optimizations and code generation. A special compiler temporary is
   * one whose location in frame is well known while non-special ones
   * do not have a requirement on location in frame as long as code
   * generator itself knows how to access them.
   *
   *     +-------------------------------+
   *     | IN[ins-1]                     |  {Note: resides in caller's frame}
   *     |       .                       |
   *     | IN[0]                         |
   *     | caller's ArtMethod            |  ... ArtMethod*
   *     +===============================+  {Note: start of callee's frame}
   *     | core callee-save spill        |  {variable sized}
   *     +-------------------------------+
   *     | fp callee-save spill          |
   *     +-------------------------------+
   *     | filler word                   |  {For compatibility, if V[locals-1] used as wide
   *     +-------------------------------+
   *     | V[locals-1]                   |
   *     | V[locals-2]                   |
   *     |      .                        |
   *     |      .                        |  ... (reg == 2)
   *     | V[1]                          |  ... (reg == 1)
   *     | V[0]                          |  ... (reg == 0) <---- "locals_start"
   *     +-------------------------------+
   *     | stack alignment padding       |  {0 to (kStackAlignWords-1) of padding}
   *     +-------------------------------+
   *     | Compiler temp region          |  ... (reg >= max_num_special_temps)
   *     |      .                        |
   *     |      .                        |
   *     | V[max_num_special_temps + 1]  |
   *     | V[max_num_special_temps + 0]  |
   *     +-------------------------------+
   *     | OUT[outs-1]                   |
   *     | OUT[outs-2]                   |
   *     |       .                       |
   *     | OUT[0]                        |
   *     | ArtMethod*                    |  ... (reg == num_total_code_regs == special_temp_value) <<== sp, 16-byte aligned
   *     +===============================+
   */
  static int GetVRegOffsetFromQuickCode(const DexFile::CodeItem* code_item,
                                        uint32_t core_spills, uint32_t fp_spills,
                                        size_t frame_size, int reg, InstructionSet isa);

  static int GetOutVROffset(uint16_t out_num, InstructionSet isa) {
    // According to stack model, the first out is above the Method referernce.
    return InstructionSetPointerSize(isa) + out_num * sizeof(uint32_t);
  }

  bool IsInInlinedFrame() const {
    return current_inlining_depth_ != 0;
  }

  size_t GetCurrentInliningDepth() const {
    return current_inlining_depth_;
  }

  uintptr_t GetCurrentQuickFramePc() const {
    return cur_quick_frame_pc_;
  }

  ArtMethod** GetCurrentQuickFrame() const {
    return cur_quick_frame_;
  }

  ShadowFrame* GetCurrentShadowFrame() const {
    return cur_shadow_frame_;
  }

  bool IsCurrentFrameInInterpreter() const {
    return cur_shadow_frame_ != nullptr;
  }

  HandleScope* GetCurrentHandleScope(size_t pointer_size) const {
    ArtMethod** sp = GetCurrentQuickFrame();
    // Skip ArtMethod*; handle scope comes next;
    return reinterpret_cast<HandleScope*>(reinterpret_cast<uintptr_t>(sp) + pointer_size);
  }

  std::string DescribeLocation() const SHARED_REQUIRES(Locks::mutator_lock_);

  static size_t ComputeNumFrames(Thread* thread, StackWalkKind walk_kind)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static void DescribeStack(Thread* thread) SHARED_REQUIRES(Locks::mutator_lock_);

  const OatQuickMethodHeader* GetCurrentOatQuickMethodHeader() const {
    return cur_oat_quick_method_header_;
  }

  QuickMethodFrameInfo GetCurrentQuickFrameInfo() const SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  // Private constructor known in the case that num_frames_ has already been computed.
  StackVisitor(Thread* thread, Context* context, StackWalkKind walk_kind, size_t num_frames)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsAccessibleRegister(uint32_t reg, bool is_float) const {
    return is_float ? IsAccessibleFPR(reg) : IsAccessibleGPR(reg);
  }
  uintptr_t GetRegister(uint32_t reg, bool is_float) const {
    DCHECK(IsAccessibleRegister(reg, is_float));
    return is_float ? GetFPR(reg) : GetGPR(reg);
  }

  bool IsAccessibleGPR(uint32_t reg) const;
  uintptr_t GetGPR(uint32_t reg) const;

  bool IsAccessibleFPR(uint32_t reg) const;
  uintptr_t GetFPR(uint32_t reg) const;

  bool GetVRegFromDebuggerShadowFrame(uint16_t vreg, VRegKind kind, uint32_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool GetVRegFromOptimizedCode(ArtMethod* m, uint16_t vreg, VRegKind kind,
                                uint32_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool GetVRegPairFromDebuggerShadowFrame(uint16_t vreg, VRegKind kind_lo, VRegKind kind_hi,
                                          uint64_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool GetVRegPairFromOptimizedCode(ArtMethod* m, uint16_t vreg,
                                    VRegKind kind_lo, VRegKind kind_hi,
                                    uint64_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool GetRegisterPairIfAccessible(uint32_t reg_lo, uint32_t reg_hi, VRegKind kind_lo,
                                   uint64_t* val) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  void SanityCheckFrame() const SHARED_REQUIRES(Locks::mutator_lock_);

  InlineInfo GetCurrentInlineInfo() const SHARED_REQUIRES(Locks::mutator_lock_);

  Thread* const thread_;
  const StackWalkKind walk_kind_;
  ShadowFrame* cur_shadow_frame_;
  ArtMethod** cur_quick_frame_;
  uintptr_t cur_quick_frame_pc_;
  const OatQuickMethodHeader* cur_oat_quick_method_header_;
  // Lazily computed, number of frames in the stack.
  size_t num_frames_;
  // Depth of the frame we're currently at.
  size_t cur_depth_;
  // Current inlining depth of the method we are currently at.
  // 0 if there is no inlined frame.
  size_t current_inlining_depth_;

 protected:
  Context* const context_;
};

}  // namespace art

#endif  // ART_RUNTIME_STACK_H_
