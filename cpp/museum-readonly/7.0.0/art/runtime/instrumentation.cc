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

#include <museum/7.0.0/art/runtime/instrumentation.h>

#include <museum/7.0.0/external/libcxx/sstream>

#include <museum/7.0.0/art/runtime/arch/context.h>
#include <museum/7.0.0/art/runtime/art_method-inl.h>
#include <museum/7.0.0/art/runtime/atomic.h>
#include <museum/7.0.0/art/runtime/class_linker.h>
#include <museum/7.0.0/art/runtime/debugger.h>
#include <museum/7.0.0/art/runtime/dex_file-inl.h>
#include <museum/7.0.0/art/runtime/entrypoints/quick/quick_entrypoints.h>
#include <museum/7.0.0/art/runtime/entrypoints/quick/quick_alloc_entrypoints.h>
#include <museum/7.0.0/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/7.0.0/art/runtime/gc_root-inl.h>
#include <museum/7.0.0/art/runtime/interpreter/interpreter.h>
//#include <museum/7.0.0/art/runtime/jit/jit.h>
//#include <museum/7.0.0/art/runtime/jit/jit_code_cache.h>
#include <museum/7.0.0/art/runtime/mirror/class-inl.h>
#include <museum/7.0.0/art/runtime/mirror/dex_cache.h>
#include <museum/7.0.0/art/runtime/mirror/object_array-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object-inl.h>
#include <museum/7.0.0/art/runtime/nth_caller_visitor.h>
#include <museum/7.0.0/art/runtime/oat_quick_method_header.h>
#include <museum/7.0.0/art/runtime/thread.h>
#include <museum/7.0.0/art/runtime/thread_list.h>

#include <new>

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
