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

#include <museum/6.0.1/art/runtime/class_linker.h>

#include <museum/6.0.1/external/libcxx/deque>
#include <museum/6.0.1/external/libcxx/iostream>
#include <museum/6.0.1/external/libcxx/memory>
#include <museum/6.0.1/external/libcxx/queue>
#include <museum/6.0.1/external/libcxx/string>
#include <museum/6.0.1/bionic/libc/unistd.h>
#include <museum/6.0.1/external/libcxx/utility>
#include <museum/6.0.1/external/libcxx/vector>

#include <museum/6.0.1/art/runtime/art_field-inl.h>
#include <museum/6.0.1/art/runtime/art_method-inl.h>
#include <museum/6.0.1/art/runtime/base/arena_allocator.h>
#include <museum/6.0.1/art/runtime/base/casts.h>
#include <museum/6.0.1/art/runtime/base/logging.h>
#include <museum/6.0.1/art/runtime/base/scoped_arena_containers.h>
#include <museum/6.0.1/art/runtime/base/scoped_flock.h>
#include <museum/6.0.1/art/runtime/base/stl_util.h>
#include <museum/6.0.1/art/runtime/base/time_utils.h>
#include <museum/6.0.1/art/runtime/base/unix_file/fd_file.h>
#include <museum/6.0.1/art/runtime/base/value_object.h>
#include <museum/6.0.1/art/runtime/class_linker-inl.h>
#include <museum/6.0.1/art/runtime/compiler_callbacks.h>
#include <museum/6.0.1/art/runtime/debugger.h>
#include <museum/6.0.1/art/runtime/dex_file-inl.h>
#include <museum/6.0.1/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/6.0.1/art/runtime/gc_root-inl.h>
#include <museum/6.0.1/art/runtime/gc/accounting/card_table-inl.h>
#include <museum/6.0.1/art/runtime/gc/accounting/heap_bitmap.h>
#include <museum/6.0.1/art/runtime/gc/heap.h>
#include <museum/6.0.1/art/runtime/gc/space/image_space.h>
#include <museum/6.0.1/art/runtime/handle_scope.h>
#include <museum/6.0.1/art/runtime/intern_table.h>
#include <museum/6.0.1/art/runtime/interpreter/interpreter.h>
#include <museum/6.0.1/art/runtime/jit/jit.h>
#include <museum/6.0.1/art/runtime/jit/jit_code_cache.h>
#include <museum/6.0.1/art/runtime/leb128.h>
#include <museum/6.0.1/art/runtime/linear_alloc.h>
#include <museum/6.0.1/art/runtime/oat.h>
#include <museum/6.0.1/art/runtime/oat_file.h>
#include <museum/6.0.1/art/runtime/oat_file-inl.h>
#include <museum/6.0.1/art/runtime/oat_file_assistant.h>
#include <museum/6.0.1/art/runtime/object_lock.h>
#include <museum/6.0.1/art/runtime/mirror/class.h>
#include <museum/6.0.1/art/runtime/mirror/class-inl.h>
#include <museum/6.0.1/art/runtime/mirror/class_loader.h>
#include <museum/6.0.1/art/runtime/mirror/dex_cache-inl.h>
#include <museum/6.0.1/art/runtime/mirror/field.h>
#include <museum/6.0.1/art/runtime/mirror/iftable-inl.h>
#include <museum/6.0.1/art/runtime/mirror/method.h>
#include <museum/6.0.1/art/runtime/mirror/object-inl.h>
#include <museum/6.0.1/art/runtime/mirror/object_array-inl.h>
#include <museum/6.0.1/art/runtime/mirror/proxy.h>
#include <museum/6.0.1/art/runtime/mirror/reference-inl.h>
#include <museum/6.0.1/art/runtime/mirror/stack_trace_element.h>
#include <museum/6.0.1/art/runtime/mirror/string-inl.h>
#include <museum/6.0.1/art/runtime/os.h>
#include <museum/6.0.1/art/runtime/runtime.h>
#include <museum/6.0.1/art/runtime/entrypoints/entrypoint_utils.h>
//#include <museum/6.0.1/art/runtime/ScopedLocalRef.h>
#include <museum/6.0.1/art/runtime/scoped_thread_state_change.h>
#include <museum/6.0.1/art/runtime/handle_scope-inl.h>
#include <museum/6.0.1/art/runtime/thread-inl.h>
#include <museum/6.0.1/art/runtime/utils.h>
#include <museum/6.0.1/art/runtime/verifier/method_verifier.h>
#include <museum/6.0.1/art/runtime/well_known_classes.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

const void* ClassLinker::GetQuickOatCodeFor(ArtMethod* method) {
  //CHECK(!method->IsAbstract()) << PrettyMethod(method);
  if (method->IsProxyMethod()) {
    return GetQuickProxyInvokeHandler();
  }
  bool found;
  OatFile::OatMethod oat_method = FindOatMethodFor(method, &found);
  if (found) {
    auto* code = oat_method.GetQuickCode();
    if (code != nullptr) {
      return code;
    }
  }
  jit::Jit* const jit = Runtime::Current()->GetJit();
  if (jit != nullptr) {
    auto* code = jit->GetCodeCache()->GetCodeFor(method);
    if (code != nullptr) {
      return code;
    }
  }
  if (method->IsNative()) {
    // No code and native? Use generic trampoline.
    return GetQuickGenericJniStub();
  }
  return GetQuickToInterpreterBridge();
}

bool ClassLinker::IsQuickResolutionStub(const void* entry_point) const {
  return (entry_point == GetQuickResolutionStub()) ||
      (quick_resolution_trampoline_ == entry_point);
}

bool ClassLinker::IsQuickToInterpreterBridge(const void* entry_point) const {
  return (entry_point == GetQuickToInterpreterBridge()) ||
      (quick_to_interpreter_bridge_trampoline_ == entry_point);
}

bool ClassLinker::IsQuickGenericJniStub(const void* entry_point) const {
  return (entry_point == GetQuickGenericJniStub()) ||
      (quick_generic_jni_trampoline_ == entry_point);
}

const void* ClassLinker::GetRuntimeQuickGenericJniStub() const {
  return GetQuickGenericJniStub();
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
