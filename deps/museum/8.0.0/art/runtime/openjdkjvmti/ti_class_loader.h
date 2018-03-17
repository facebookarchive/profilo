/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_LOADER_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_LOADER_H_

#include <museum/8.0.0/external/libcxx/string>

#include <museum/8.0.0/libnativehelper/jni.h>

#include <museum/8.0.0/art/runtime/openjdkjvmti/art_jvmti.h>
#include <museum/8.0.0/art/runtime/art_method.h>
#include <museum/8.0.0/art/runtime/base/array_slice.h>
#include <museum/8.0.0/art/runtime/class_linker.h>
#include <museum/8.0.0/art/runtime/dex_file.h>
#include <museum/8.0.0/art/runtime/gc_root-inl.h>
#include <museum/8.0.0/art/runtime/globals.h>
#include <museum/8.0.0/art/runtime/jni_env_ext-inl.h>
#include "jvmti.h"
#include <museum/8.0.0/art/runtime/linear_alloc.h>
#include <museum/8.0.0/art/runtime/mem_map.h>
#include <museum/8.0.0/art/runtime/mirror/array-inl.h>
#include <museum/8.0.0/art/runtime/mirror/array.h>
#include <museum/8.0.0/art/runtime/mirror/class-inl.h>
#include <museum/8.0.0/art/runtime/mirror/class.h>
#include <museum/8.0.0/art/runtime/mirror/class_loader-inl.h>
#include <museum/8.0.0/art/runtime/mirror/string-inl.h>
#include <museum/8.0.0/art/runtime/oat_file.h>
#include <museum/8.0.0/art/runtime/obj_ptr.h>
#include <museum/8.0.0/art/runtime/scoped_thread_state_change-inl.h>
#include <museum/8.0.0/art/runtime/stack.h>
#include <museum/8.0.0/art/runtime/openjdkjvmti/ti_class_definition.h>
#include <museum/8.0.0/art/runtime/thread_list.h>
#include <museum/8.0.0/art/runtime/openjdkjvmti/transform.h>
#include <museum/8.0.0/art/runtime/utf.h>
#include <museum/8.0.0/art/runtime/utils/dex_cache_arrays_layout-inl.h>

namespace openjdkjvmti {

// Class that can redefine a single class's methods.
// TODO We should really make this be driven by an outside class so we can do multiple classes at
// the same time and have less required cleanup.
class ClassLoaderHelper {
 public:
  static bool AddToClassLoader(facebook::museum::MUSEUM_VERSION::art::Thread* self,
                               facebook::museum::MUSEUM_VERSION::art::Handle<facebook::museum::MUSEUM_VERSION::art::mirror::ClassLoader> loader,
                               const facebook::museum::MUSEUM_VERSION::art::DexFile* dex_file)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_);

  // Finds a java.lang.DexFile object that is associated with the given ClassLoader. Each of these
  // j.l.DexFile objects holds several facebook::museum::MUSEUM_VERSION::art::DexFile*s in it.
  // TODO This should return the actual source java.lang.DexFile object for the klass being loaded.
  static facebook::museum::MUSEUM_VERSION::art::ObjPtr<facebook::museum::MUSEUM_VERSION::art::mirror::Object> FindSourceDexFileObject(
      facebook::museum::MUSEUM_VERSION::art::Thread* self, facebook::museum::MUSEUM_VERSION::art::Handle<facebook::museum::MUSEUM_VERSION::art::mirror::ClassLoader> loader)
      REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_);

  static facebook::museum::MUSEUM_VERSION::art::ObjPtr<facebook::museum::MUSEUM_VERSION::art::mirror::LongArray> GetDexFileCookie(
      facebook::museum::MUSEUM_VERSION::art::Handle<facebook::museum::MUSEUM_VERSION::art::mirror::Object> java_dex_file) REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_);

  static facebook::museum::MUSEUM_VERSION::art::ObjPtr<facebook::museum::MUSEUM_VERSION::art::mirror::LongArray> AllocateNewDexFileCookie(
      facebook::museum::MUSEUM_VERSION::art::Thread* self,
      facebook::museum::MUSEUM_VERSION::art::Handle<facebook::museum::MUSEUM_VERSION::art::mirror::LongArray> old_dex_file_cookie,
      const facebook::museum::MUSEUM_VERSION::art::DexFile* new_dex_file) REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_);

  static void UpdateJavaDexFile(facebook::museum::MUSEUM_VERSION::art::ObjPtr<facebook::museum::MUSEUM_VERSION::art::mirror::Object> java_dex_file,
                                facebook::museum::MUSEUM_VERSION::art::ObjPtr<facebook::museum::MUSEUM_VERSION::art::mirror::LongArray> new_cookie)
      REQUIRES(facebook::museum::MUSEUM_VERSION::art::Roles::uninterruptible_) REQUIRES_SHARED(facebook::museum::MUSEUM_VERSION::art::Locks::mutator_lock_);
};

}  // namespace openjdkjvmti
#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_LOADER_H_
