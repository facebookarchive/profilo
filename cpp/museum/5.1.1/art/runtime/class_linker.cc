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

#include <museum/5.1.1/art/runtime/class_linker.h>

#include <museum/5.1.1/bionic/libc/fcntl.h>
#include <museum/5.1.1/bionic/libc/sys/file.h>
#include <museum/5.1.1/bionic/libc/sys/stat.h>
#include <museum/5.1.1/bionic/libc/unistd.h>
#include <museum/5.1.1/external/libcxx/deque>
#include <museum/5.1.1/external/libcxx/memory>
#include <museum/5.1.1/external/libcxx/string>
#include <museum/5.1.1/external/libcxx/utility>
#include <museum/5.1.1/external/libcxx/vector>

#include <museum/5.1.1/art/runtime/base/casts.h>
#include <museum/5.1.1/art/runtime/base/logging.h>
#include <museum/5.1.1/art/runtime/base/scoped_flock.h>
#include <museum/5.1.1/art/runtime/base/stl_util.h>
#include <museum/5.1.1/art/runtime/base/unix_file/fd_file.h>
#include <museum/5.1.1/art/runtime/class_linker-inl.h>
#include <museum/5.1.1/art/runtime/compiler_callbacks.h>
#include <museum/5.1.1/art/runtime/debugger.h>
#include <museum/5.1.1/art/runtime/dex_file-inl.h>
#include <museum/5.1.1/art/runtime/gc_root-inl.h>
#include <museum/5.1.1/art/runtime/gc/accounting/card_table-inl.h>
#include <museum/5.1.1/art/runtime/gc/accounting/heap_bitmap.h>
#include <museum/5.1.1/art/runtime/gc/heap.h>
#include <museum/5.1.1/art/runtime/gc/space/image_space.h>
#include <museum/5.1.1/art/runtime/handle_scope.h>
#include <museum/5.1.1/art/runtime/intern_table.h>
#include <museum/5.1.1/art/runtime/interpreter/interpreter.h>
#include <museum/5.1.1/art/runtime/leb128.h>
#include <museum/5.1.1/art/runtime/method_helper-inl.h>
#include <museum/5.1.1/art/runtime/oat.h>
#include <museum/5.1.1/art/runtime/oat_file.h>
#include <museum/5.1.1/art/runtime/object_lock.h>
#include <museum/5.1.1/art/runtime/mirror/art_field-inl.h>
#include <museum/5.1.1/art/runtime/mirror/art_method-inl.h>
#include <museum/5.1.1/art/runtime/mirror/class.h>
#include <museum/5.1.1/art/runtime/mirror/class-inl.h>
#include <museum/5.1.1/art/runtime/mirror/class_loader.h>
#include <museum/5.1.1/art/runtime/mirror/dex_cache-inl.h>
#include <museum/5.1.1/art/runtime/mirror/iftable-inl.h>
#include <museum/5.1.1/art/runtime/mirror/object-inl.h>
#include <museum/5.1.1/art/runtime/mirror/object_array-inl.h>
#include <museum/5.1.1/art/runtime/mirror/proxy.h>
#include <museum/5.1.1/art/runtime/mirror/reference-inl.h>
#include <museum/5.1.1/art/runtime/mirror/stack_trace_element.h>
#include <museum/5.1.1/art/runtime/mirror/string-inl.h>
#include <museum/5.1.1/art/runtime/os.h>
#include <museum/5.1.1/art/runtime/runtime.h>
#include <museum/5.1.1/art/runtime/entrypoints/entrypoint_utils.h>
//#include <museum/5.1.1/art/runtime/ScopedLocalRef.h>
#include <museum/5.1.1/art/runtime/scoped_thread_state_change.h>
#include <museum/5.1.1/art/runtime/handle_scope-inl.h>
#include <museum/5.1.1/art/runtime/thread.h>
#include <museum/5.1.1/art/runtime/utils.h>
#include <museum/5.1.1/art/runtime/verifier/method_verifier.h>
#include <museum/5.1.1/art/runtime/well_known_classes.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

// Special case to get oat code without overwriting a trampoline.
const void* ClassLinker::GetQuickOatCodeFor(mirror::ArtMethod* method) {
  // CHECK(!method->IsAbstract()) << PrettyMethod(method);
  if (method->IsProxyMethod()) {
    return GetQuickProxyInvokeHandler();
  }
  OatFile::OatMethod oat_method;
  const void* result = nullptr;
  if (FindOatMethodFor(method, &oat_method)) {
    result = oat_method.GetQuickCode();
  }

  if (result == nullptr) {
    if (method->IsNative()) {
      // No code and native? Use generic trampoline.
      result = GetQuickGenericJniTrampoline();
#if defined(ART_USE_PORTABLE_COMPILER)
    } else if (method->IsPortableCompiled()) {
      // No code? Do we expect portable code?
      result = GetQuickToPortableBridge();
#endif
    } else {
      // No code? You must mean to go into the interpreter.
      result = GetQuickToInterpreterBridge();
    }
  }
  return result;
}
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
