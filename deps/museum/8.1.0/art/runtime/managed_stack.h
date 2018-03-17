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

#ifndef ART_RUNTIME_MANAGED_STACK_H_
#define ART_RUNTIME_MANAGED_STACK_H_

#include <cstring>
#include <stdint.h>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"

namespace art {

namespace mirror {
class Object;
}  // namespace mirror

class ArtMethod;
class ShadowFrame;
template <typename T> class StackReference;

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

  ALWAYS_INLINE ShadowFrame* PushShadowFrame(ShadowFrame* new_top_frame);
  ALWAYS_INLINE ShadowFrame* PopShadowFrame();

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

  size_t NumJniShadowFrameReferences() const REQUIRES_SHARED(Locks::mutator_lock_);

  bool ShadowFramesContain(StackReference<mirror::Object>* shadow_frame_entry) const;

 private:
  ArtMethod** top_quick_frame_;
  ManagedStack* link_;
  ShadowFrame* top_shadow_frame_;
};

}  // namespace art

#endif  // ART_RUNTIME_MANAGED_STACK_H_
