/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_RUNTIME_OPTIONS_H_
#define ART_RUNTIME_RUNTIME_OPTIONS_H_

#include "runtime/base/variant_map.h"
#include "cmdline/cmdline_types.h"  // TODO: don't need to include this file here

// Map keys
#include <vector>
#include <string>
#include "runtime/base/logging.h"
#include "cmdline/unit.h"
#include "jdwp/jdwp.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "gc/collector_type.h"
#include "gc/space/large_object_space.h"
#include "profiler_options.h"
#include "arch/instruction_set.h"
#include <stdio.h>
#include <stdarg.h>

namespace art {

class CompilerCallbacks;
class DexFile;
struct XGcOption;
struct BackgroundGcOption;
struct TestProfilerOptions;

#define DECLARE_KEY(Type, Name) static const Key<Type> Name

  // Define a key that is usable with a RuntimeArgumentMap.
  // This key will *not* work with other subtypes of VariantMap.
  template <typename TValue>
  struct RuntimeArgumentMapKey : VariantMapKey<TValue> {
    RuntimeArgumentMapKey() {}
    explicit RuntimeArgumentMapKey(TValue default_value)
      : VariantMapKey<TValue>(std::move(default_value)) {}
    // Don't ODR-use constexpr default values, which means that Struct::Fields
    // that are declared 'static constexpr T Name = Value' don't need to have a matching definition.
  };

  // Defines a type-safe heterogeneous key->value map.
  // Use the VariantMap interface to look up or to store a RuntimeArgumentMapKey,Value pair.
  //
  // Example:
  //    auto map = RuntimeArgumentMap();
  //    map.Set(RuntimeArgumentMap::HeapTargetUtilization, 5.0);
  //    double *target_utilization = map.Get(RuntimeArgumentMap);
  //
  struct RuntimeArgumentMap : VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey> {
    // This 'using' line is necessary to inherit the variadic constructor.
    using VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey>::VariantMap;

    // Make the next many usages of Key slightly shorter to type.
    template <typename TValue>
    using Key = RuntimeArgumentMapKey<TValue>;

    // List of key declarations, shorthand for 'static const Key<T> Name'
#define RUNTIME_OPTIONS_KEY(Type, Name, ...) static const Key<Type> Name;
#include "runtime_options.def"
  };

#undef DECLARE_KEY

  // using RuntimeOptions = RuntimeArgumentMap;
}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_OPTIONS_H_
