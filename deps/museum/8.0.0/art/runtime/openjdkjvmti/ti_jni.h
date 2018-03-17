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

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_JNI_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_JNI_H_

#include "jni.h"
#include "jvmti.h"

namespace openjdkjvmti {

// Note: Currently, JNI function table changes are sensitive to the order of operations wrt/
//       CheckJNI. If an agent sets the function table, and a program than late-enables CheckJNI,
//       CheckJNI will not be working (as the agent will forward to the non-CheckJNI table).
//
//       This behavior results from our usage of the function table to avoid a check of the
//       CheckJNI flag. A future implementation may install on loading of this plugin an
//       intermediate function table that explicitly checks the flag, so that switching CheckJNI
//       is transparently handled.

class JNIUtil {
 public:
  static jvmtiError SetJNIFunctionTable(jvmtiEnv* env, const jniNativeInterface* function_table);

  static jvmtiError GetJNIFunctionTable(jvmtiEnv* env, jniNativeInterface** function_table);
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_JNI_H_
