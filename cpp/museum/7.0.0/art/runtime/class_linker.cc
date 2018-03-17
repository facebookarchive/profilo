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

#include <museum/7.0.0/art/runtime/class_linker.h>

#include <museum/7.0.0/bionic/libc/unistd.h>

#include <museum/7.0.0/external/libcxx/algorithm>
#include <museum/7.0.0/external/libcxx/deque>
#include <museum/7.0.0/external/libcxx/iostream>
#include <museum/7.0.0/external/libcxx/memory>
#include <museum/7.0.0/external/libcxx/queue>
#include <museum/7.0.0/external/libcxx/string>
#include <museum/7.0.0/external/libcxx/tuple>
#include <museum/7.0.0/external/libcxx/unordered_map>
#include <museum/7.0.0/external/libcxx/utility>
#include <museum/7.0.0/external/libcxx/vector>

#include <museum/7.0.0/art/runtime/art_field-inl.h>
#include <museum/7.0.0/art/runtime/art_method-inl.h>
#include <museum/7.0.0/art/runtime/base/arena_allocator.h>
#include <museum/7.0.0/art/runtime/base/casts.h>
#include <museum/7.0.0/art/runtime/base/logging.h>
#include <museum/7.0.0/art/runtime/base/scoped_arena_containers.h>
#include <museum/7.0.0/art/runtime/base/scoped_flock.h>
#include <museum/7.0.0/art/runtime/base/stl_util.h>
//#include <museum/7.0.0/art/runtime/base/systrace.h>
#include <museum/7.0.0/art/runtime/base/time_utils.h>
#include <museum/7.0.0/art/runtime/base/unix_file/fd_file.h>
#include <museum/7.0.0/art/runtime/base/value_object.h>
#include <museum/7.0.0/art/runtime/class_linker-inl.h>
#include <museum/7.0.0/art/runtime/class_table-inl.h>
#include <museum/7.0.0/art/runtime/compiler_callbacks.h>
#include <museum/7.0.0/art/runtime/debugger.h>
#include <museum/7.0.0/art/runtime/dex_file-inl.h>
#include <museum/7.0.0/art/runtime/entrypoints/entrypoint_utils.h>
#include <museum/7.0.0/art/runtime/entrypoints/runtime_asm_entrypoints.h>
#include <museum/7.0.0/art/runtime/experimental_flags.h>
#include <museum/7.0.0/art/runtime/gc_root-inl.h>
#include <museum/7.0.0/art/runtime/gc/accounting/card_table-inl.h>
#include <museum/7.0.0/art/runtime/gc/accounting/heap_bitmap-inl.h>
#include <museum/7.0.0/art/runtime/gc/heap.h>
#include <museum/7.0.0/art/runtime/gc/scoped_gc_critical_section.h>
#include <museum/7.0.0/art/runtime/gc/space/image_space.h>
#include <museum/7.0.0/art/runtime/handle_scope-inl.h>
#include <museum/7.0.0/art/runtime/image-inl.h>
#include <museum/7.0.0/art/runtime/intern_table.h>
#include <museum/7.0.0/art/runtime/interpreter/interpreter.h>
#include <museum/7.0.0/art/runtime/jit/jit.h>
#include <museum/7.0.0/art/runtime/jit/jit_code_cache.h>
#include <museum/7.0.0/art/runtime/jit/offline_profiling_info.h>
#include <museum/7.0.0/art/runtime/leb128.h>
#include <museum/7.0.0/art/runtime/linear_alloc.h>
#include <museum/7.0.0/art/runtime/mirror/class.h>
#include <museum/7.0.0/art/runtime/mirror/class-inl.h>
#include <museum/7.0.0/art/runtime/mirror/class_loader.h>
#include <museum/7.0.0/art/runtime/mirror/dex_cache-inl.h>
#include <museum/7.0.0/art/runtime/mirror/field.h>
#include <museum/7.0.0/art/runtime/mirror/iftable-inl.h>
#include <museum/7.0.0/art/runtime/mirror/method.h>
#include <museum/7.0.0/art/runtime/mirror/object-inl.h>
#include <museum/7.0.0/art/runtime/mirror/object_array-inl.h>
#include <museum/7.0.0/art/runtime/mirror/proxy.h>
#include <museum/7.0.0/art/runtime/mirror/reference-inl.h>
#include <museum/7.0.0/art/runtime/mirror/stack_trace_element.h>
#include <museum/7.0.0/art/runtime/mirror/string-inl.h>
#include <museum/7.0.0/art/runtime/native/dalvik_system_DexFile.h>
#include <museum/7.0.0/art/runtime/oat.h>
#include <museum/7.0.0/art/runtime/oat_file.h>
#include <museum/7.0.0/art/runtime/oat_file-inl.h>
#include <museum/7.0.0/art/runtime/oat_file_assistant.h>
#include <museum/7.0.0/art/runtime/oat_file_manager.h>
#include <museum/7.0.0/art/runtime/object_lock.h>
#include <museum/7.0.0/art/runtime/os.h>
#include <museum/7.0.0/art/runtime/runtime.h>
//#include <museum/7.0.0/art/runtime/ScopedLocalRef.h>
#include <museum/7.0.0/art/runtime/scoped_thread_state_change.h>
#include <museum/7.0.0/art/runtime/thread-inl.h>
#include <museum/7.0.0/art/runtime/trace.h>
#include <museum/7.0.0/art/runtime/utils.h>
#include <museum/7.0.0/art/runtime/utils/dex_cache_arrays_layout-inl.h>
#include <museum/7.0.0/art/runtime/verifier/method_verifier.h>
#include <museum/7.0.0/art/runtime/well_known_classes.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

static constexpr bool kSanityCheckObjects = kIsDebugBuild;
static constexpr bool kVerifyArtMethodDeclaringClasses = kIsDebugBuild;

OatFile::OatClass ClassLinker::FindOatClass(const DexFile& dex_file,
                                            uint16_t class_def_idx,
                                            bool* found) {
  DCHECK_NE(class_def_idx, DexFile::kDexNoIndex16);
  const OatFile::OatDexFile* oat_dex_file = dex_file.GetOatDexFile();
  if (oat_dex_file == nullptr) {
    *found = false;
    return OatFile::OatClass::Invalid();
  }
  *found = true;
  return oat_dex_file->GetOatClass(class_def_idx);
}

const OatFile::OatMethod ClassLinker::FindOatMethodFor(ArtMethod* method, bool* found) {
  // Although we overwrite the trampoline of non-static methods, we may get here via the resolution
  // method for direct methods (or virtual methods made direct).
  mirror::Class* declaring_class = method->GetDeclaringClass();
  size_t oat_method_index;
  if (method->IsStatic() || method->IsDirect()) {
    // Simple case where the oat method index was stashed at load time.
    oat_method_index = method->GetMethodIndex();
  } else {
    // We're invoking a virtual method directly (thanks to sharpening), compute the oat_method_index
    // by search for its position in the declared virtual methods.
    oat_method_index = declaring_class->NumDirectMethods();
    bool found_virtual = false;
    for (ArtMethod& art_method : declaring_class->GetVirtualMethods(image_pointer_size_)) {
      // Check method index instead of identity in case of duplicate method definitions.
      if (method->GetDexMethodIndex() == art_method.GetDexMethodIndex()) {
        found_virtual = true;
        break;
      }
      oat_method_index++;
    }
    //CHECK(found_virtual) << "Didn't find oat method index for virtual method: "
    //                     << PrettyMethod(method);
  }
  //DCHECK_EQ(oat_method_index,
  //          GetOatMethodIndexFromMethodIndex(*declaring_class->GetDexCache()->GetDexFile(),
  //                                           method->GetDeclaringClass()->GetDexClassDefIndex(),
  //                                           method->GetDexMethodIndex()));
  OatFile::OatClass oat_class = FindOatClass(*declaring_class->GetDexCache()->GetDexFile(),
                                             declaring_class->GetDexClassDefIndex(),
                                             found);
  if (!(*found)) {
    return OatFile::OatMethod::Invalid();
  }
  return oat_class.GetOatMethod(oat_method_index);
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

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
