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

#include "instrumentation.h"

#include <sstream>

#include "arch/context.h"
#include "art_method-inl.h"
#include "atomic.h"
#include "class_linker.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "gc_root-inl.h"
#include "interpreter/interpreter.h"
//#include "jit/jit.h"
//#include "jit/jit_code_cache.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#include "nth_caller_visitor.h"
#include "oat_quick_method_header.h"
#include "thread.h"
#include "thread_list.h"

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace instrumentation {

// Instrumentation works on non-inlined frames by updating returned PCs
// of compiled frames.
static constexpr StackVisitor::StackWalkKind kInstrumentationStackWalk =
    StackVisitor::StackWalkKind::kSkipInlinedFrames;

const void* Instrumentation::GetQuickCodeFor(ArtMethod* method, size_t pointer_size) const {
  Runtime* runtime = Runtime::Current();
  if (LIKELY(!instrumentation_stubs_installed_)) {
    const void* code = method->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
    DCHECK(code != nullptr);
    ClassLinker* class_linker = runtime->GetClassLinker();
    if (LIKELY(!class_linker->IsQuickResolutionStub(code) &&
               !class_linker->IsQuickToInterpreterBridge(code)) &&
               !class_linker->IsQuickResolutionStub(code) &&
               !class_linker->IsQuickToInterpreterBridge(code)) {
      return code;
    }
  }
  // FB - DO NOT SUPPORT INSTRUMENTATION HOOKS UNWINDING
  return nullptr; //return runtime->GetClassLinker()->GetQuickOatCodeFor(method);
}

// Computes a frame ID by ignoring inlined frames.
size_t Instrumentation::ComputeFrameId(Thread* self,
                                       size_t frame_depth,
                                       size_t inlined_frames_before_frame) {
  CHECK_GE(frame_depth, inlined_frames_before_frame);
  size_t no_inline_depth = frame_depth - inlined_frames_before_frame;
  return StackVisitor::ComputeNumFrames(self, kInstrumentationStackWalk) - no_inline_depth;
}

}  // namespace instrumentation
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
