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

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_BREAKPOINT_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_BREAKPOINT_H_

#include "jni.h"
#include "jvmti.h"

#include "base/mutex.h"

namespace art {
class ArtMethod;
namespace mirror {
class Class;
}  // namespace mirror
}  // namespace art

namespace openjdkjvmti {

struct ArtJvmTiEnv;

class Breakpoint {
 public:
  Breakpoint(art::ArtMethod* m, jlocation loc);

  // Get the hash code of this breakpoint.
  size_t hash() const;

  bool operator==(const Breakpoint& other) const {
    return method_ == other.method_ && location_ == other.location_;
  }

  art::ArtMethod* GetMethod() const {
    return method_;
  }

  jlocation GetLocation() const {
    return location_;
  }

 private:
  art::ArtMethod* method_;
  jlocation location_;
};

class BreakpointUtil {
 public:
  static jvmtiError SetBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location);
  static jvmtiError ClearBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location);
  // Used by class redefinition to remove breakpoints on redefined classes.
  static void RemoveBreakpointsInClass(ArtJvmTiEnv* env, art::mirror::Class* klass)
      REQUIRES(art::Locks::mutator_lock_);
};

}  // namespace openjdkjvmti

namespace std {
template<> struct hash<openjdkjvmti::Breakpoint> {
  size_t operator()(const openjdkjvmti::Breakpoint& b) const {
    return b.hash();
  }
};

}  // namespace std
#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_BREAKPOINT_H_
