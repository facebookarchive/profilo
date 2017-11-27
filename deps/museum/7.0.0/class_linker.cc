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

#include "class_linker.h"

#include <algorithm>
#include <deque>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/arena_allocator.h"
#include "base/casts.h"
#include "base/logging.h"
#include "base/scoped_arena_containers.h"
#include "base/scoped_flock.h"
#include "base/stl_util.h"
//#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "base/value_object.h"
#include "class_linker-inl.h"
#include "class_table-inl.h"
#include "compiler_callbacks.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "experimental_flags.h"
#include "gc_root-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap-inl.h"
#include "gc/heap.h"
#include "gc/scoped_gc_critical_section.h"
#include "gc/space/image_space.h"
#include "handle_scope-inl.h"
#include "image-inl.h"
#include "intern_table.h"
#include "interpreter/interpreter.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jit/offline_profiling_info.h"
#include "leb128.h"
#include "linear_alloc.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/field.h"
#include "mirror/iftable-inl.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/proxy.h"
#include "mirror/reference-inl.h"
#include "mirror/stack_trace_element.h"
#include "mirror/string-inl.h"
#include "native/dalvik_system_DexFile.h"
#include "oat.h"
#include "oat_file.h"
#include "oat_file-inl.h"
#include "oat_file_assistant.h"
#include "oat_file_manager.h"
#include "object_lock.h"
#include "os.h"
#include "runtime.h"
//#include "ScopedLocalRef.h"
#include "scoped_thread_state_change.h"
#include "thread-inl.h"
#include "trace.h"
#include "utils.h"
#include "utils/dex_cache_arrays_layout-inl.h"
#include "verifier/method_verifier.h"
#include "well_known_classes.h"

namespace art {

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

}  // namespace art
