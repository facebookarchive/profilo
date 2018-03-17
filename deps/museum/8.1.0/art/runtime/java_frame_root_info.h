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

#ifndef ART_RUNTIME_JAVA_FRAME_ROOT_INFO_H_
#define ART_RUNTIME_JAVA_FRAME_ROOT_INFO_H_

#include <iosfwd>

#include "base/macros.h"
#include "base/mutex.h"
#include "gc_root.h"

namespace art {

class StackVisitor;

class JavaFrameRootInfo FINAL : public RootInfo {
 public:
  JavaFrameRootInfo(uint32_t thread_id, const StackVisitor* stack_visitor, size_t vreg)
     : RootInfo(kRootJavaFrame, thread_id), stack_visitor_(stack_visitor), vreg_(vreg) {
  }
  void Describe(std::ostream& os) const OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_);

  size_t GetVReg() const {
    return vreg_;
  }
  const StackVisitor* GetVisitor() const {
    return stack_visitor_;
  }

 private:
  const StackVisitor* const stack_visitor_;
  const size_t vreg_;
};

}  // namespace art

#endif  // ART_RUNTIME_JAVA_FRAME_ROOT_INFO_H_
