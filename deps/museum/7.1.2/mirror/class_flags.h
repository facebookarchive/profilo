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

#ifndef ART_RUNTIME_MIRROR_CLASS_FLAGS_H_
#define ART_RUNTIME_MIRROR_CLASS_FLAGS_H_

#include <stdint.h>

namespace art {
namespace mirror {

// Normal instance with at least one ref field other than the class.
static constexpr uint32_t kClassFlagNormal             = 0x00000000;

// Only normal objects which have no reference fields, e.g. string or primitive array or normal
// class instance with no fields other than klass.
static constexpr uint32_t kClassFlagNoReferenceFields  = 0x00000001;

// Class is java.lang.String.class.
static constexpr uint32_t kClassFlagString             = 0x00000004;

// Class is an object array class.
static constexpr uint32_t kClassFlagObjectArray        = 0x00000008;

// Class is java.lang.Class.class.
static constexpr uint32_t kClassFlagClass              = 0x00000010;

// Class is ClassLoader or one of its subclasses.
static constexpr uint32_t kClassFlagClassLoader        = 0x00000020;

// Class is DexCache.
static constexpr uint32_t kClassFlagDexCache           = 0x00000040;

// Class is a soft/weak/phantom class.
static constexpr uint32_t kClassFlagSoftReference      = 0x00000080;

// Class is a weak reference class.
static constexpr uint32_t kClassFlagWeakReference      = 0x00000100;

// Class is a finalizer reference class.
static constexpr uint32_t kClassFlagFinalizerReference = 0x00000200;

// Class is the phantom reference class.
static constexpr uint32_t kClassFlagPhantomReference   = 0x00000400;

// Combination of flags to figure out if the class is either the weak/soft/phantom/finalizer
// reference class.
static constexpr uint32_t kClassFlagReference =
    kClassFlagSoftReference |
    kClassFlagWeakReference |
    kClassFlagFinalizerReference |
    kClassFlagPhantomReference;

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_FLAGS_H_

