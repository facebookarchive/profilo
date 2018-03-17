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

#include <stdlib.h>
#include "class.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "class_linker.h"
#include "class_loader.h"
#include "class-inl.h"
#include "dex_cache.h"
#include "dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "handle_scope-inl.h"
#include "object_array-inl.h"
#include "object-inl.h"
#include "runtime.h"
#include "thread.h"
#include "throwable.h"
#include "utils.h"
#include "well_known_classes.h"

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace mirror {

void
Array::ThrowArrayIndexOutOfBoundsException(int32_t index)
{
  abort();
}

// Here, we've backported the ART 5.1.x FindDeclaredVirtualMethod
// with the miranda-method fix.

// See ART commit 1d0611c7e6721bd9115d652da74d2584ff3f192b:
// https://android.googlesource.com/platform/art/+/1d0611c

ArtMethod* Class::FindDeclaredVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx) {
  if (GetDexCache() == dex_cache) {
    for (size_t i = 0; i < NumVirtualMethods(); ++i) {
      ArtMethod* method = GetVirtualMethod(i);
      if (method->GetDexMethodIndex() == dex_method_idx &&
          // A miranda method may have a different DexCache and is always created by linking,
          // never *declared* in the class.
          !method->IsMiranda()) {
        return method;
      }
    }
  }
  return nullptr;
}

// We need to provide fixed copies of these methods too, since they're
// in the same translation unit as the method we're fixing and may
// contain inlined calls to the broken method; our hooking won't touch
// inlined calls.

ArtMethod* Class::FindVirtualMethod(const DexCache* dex_cache, uint32_t dex_method_idx) {
  for (Class* klass = this; klass != nullptr; klass = klass->GetSuperClass()) {
    ArtMethod* method = klass->FindDeclaredVirtualMethod(dex_cache, dex_method_idx);
    if (method != nullptr) {
      return method;
    }
  }
  return nullptr;
}

ArtMethod* Class::FindInterfaceMethod(const DexCache* dex_cache, uint32_t dex_method_idx) {
  // Check the current class before checking the interfaces.
  ArtMethod* method = FindDeclaredVirtualMethod(dex_cache, dex_method_idx);
  if (method != nullptr) {
    return method;
  }

  int32_t iftable_count = GetIfTableCount();
  IfTable* iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; ++i) {
    method = iftable->GetInterface(i)->FindDeclaredVirtualMethod(dex_cache, dex_method_idx);
    if (method != nullptr) {
      return method;
    }
  }
  return nullptr;
}

} // namespace mirror
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
