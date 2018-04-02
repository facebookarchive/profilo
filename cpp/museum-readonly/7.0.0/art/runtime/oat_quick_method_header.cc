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

#include <museum/7.0.0/art/runtime/oat_quick_method_header.h>

#include <museum/7.0.0/art/runtime/art_method.h>
#include <museum/7.0.0/art/runtime/scoped_thread_state_change.h>
#include <museum/7.0.0/art/runtime/thread.h>

#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

OatQuickMethodHeader::OatQuickMethodHeader(
    uint32_t vmap_table_offset,
    uint32_t frame_size_in_bytes,
    uint32_t core_spill_mask,
    uint32_t fp_spill_mask,
    uint32_t code_size)
    : vmap_table_offset_(vmap_table_offset),
      frame_info_(frame_size_in_bytes, core_spill_mask, fp_spill_mask),
      code_size_(code_size) {}

OatQuickMethodHeader::~OatQuickMethodHeader() {}

uint32_t OatQuickMethodHeader::ToDexPc(ArtMethod* method,
                                       const uintptr_t pc,
                                       bool abort_on_failure) const {
  const void* entry_point = GetEntryPoint();
  uint32_t sought_offset = pc - reinterpret_cast<uintptr_t>(entry_point);
  if (IsOptimized()) {
    CodeInfo code_info = GetOptimizedCodeInfo();
    CodeInfoEncoding encoding = code_info.ExtractEncoding();
    StackMap stack_map = code_info.GetStackMapForNativePcOffset(sought_offset, encoding);
    if (stack_map.IsValid()) {
      return stack_map.GetDexPc(encoding.stack_map_encoding);
    }
  } else {
    DCHECK(method->IsNative());
    return DexFile::kDexNoIndex;
  }
  if (abort_on_failure) {
    ScopedObjectAccess soa(Thread::Current());
    //LOG(FATAL) << "Failed to find Dex offset for PC offset "
    //       << reinterpret_cast<void*>(sought_offset)
    //       << "(PC " << reinterpret_cast<void*>(pc) << ", entry_point=" << entry_point
    //       << " current entry_point=" << method->GetEntryPointFromQuickCompiledCode()
    //       << ") in " << PrettyMethod(method);
  }
  return DexFile::kDexNoIndex;
}

uintptr_t OatQuickMethodHeader::ToNativeQuickPc(ArtMethod* method,
                                                const uint32_t dex_pc,
                                                bool is_for_catch_handler,
                                                bool abort_on_failure) const {
  const void* entry_point = GetEntryPoint();
  DCHECK(!method->IsNative());
  DCHECK(IsOptimized());
  // Search for the dex-to-pc mapping in stack maps.
  CodeInfo code_info = GetOptimizedCodeInfo();
  CodeInfoEncoding encoding = code_info.ExtractEncoding();

  // All stack maps are stored in the same CodeItem section, safepoint stack
  // maps first, then catch stack maps. We use `is_for_catch_handler` to select
  // the order of iteration.
  StackMap stack_map =
      LIKELY(is_for_catch_handler) ? code_info.GetCatchStackMapForDexPc(dex_pc, encoding)
                                   : code_info.GetStackMapForDexPc(dex_pc, encoding);
  if (stack_map.IsValid()) {
    return reinterpret_cast<uintptr_t>(entry_point) +
           stack_map.GetNativePcOffset(encoding.stack_map_encoding);
  }
  if (abort_on_failure) {
    ScopedObjectAccess soa(Thread::Current());
    //LOG(FATAL) << "Failed to find native offset for dex pc 0x" << std::hex << dex_pc
    //           << " in " << PrettyMethod(method);
  }
  return UINTPTR_MAX;
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
