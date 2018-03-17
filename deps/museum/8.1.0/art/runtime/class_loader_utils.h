/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_CLASS_LOADER_UTILS_H_
#define ART_RUNTIME_CLASS_LOADER_UTILS_H_

#include "handle_scope.h"
#include "mirror/class_loader.h"
#include "scoped_thread_state_change-inl.h"
#include "well_known_classes.h"

namespace art {

// Returns true if the given class loader is either a PathClassLoader or a DexClassLoader.
// (they both have the same behaviour with respect to class lockup order)
static bool IsPathOrDexClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                   Handle<mirror::ClassLoader> class_loader)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  mirror::Class* class_loader_class = class_loader->GetClass();
  return
      (class_loader_class ==
          soa.Decode<mirror::Class>(WellKnownClasses::dalvik_system_PathClassLoader)) ||
      (class_loader_class ==
          soa.Decode<mirror::Class>(WellKnownClasses::dalvik_system_DexClassLoader));
}

static bool IsDelegateLastClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                      Handle<mirror::ClassLoader> class_loader)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  mirror::Class* class_loader_class = class_loader->GetClass();
  return class_loader_class ==
      soa.Decode<mirror::Class>(WellKnownClasses::dalvik_system_DelegateLastClassLoader);
}

}  // namespace art

#endif  // ART_RUNTIME_CLASS_LOADER_UTILS_H_
