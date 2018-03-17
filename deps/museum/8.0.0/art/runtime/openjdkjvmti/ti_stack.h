/* Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_STACK_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_STACK_H_

#include "jni.h"
#include "jvmti.h"

#include "base/mutex.h"

namespace openjdkjvmti {

class StackUtil {
 public:
  static jvmtiError GetAllStackTraces(jvmtiEnv* env,
                                      jint max_frame_count,
                                      jvmtiStackInfo** stack_info_ptr,
                                      jint* thread_count_ptr)
      REQUIRES(!art::Locks::thread_list_lock_);

  static jvmtiError GetFrameCount(jvmtiEnv* env, jthread thread, jint* count_ptr);

  static jvmtiError GetFrameLocation(jvmtiEnv* env,
                                     jthread thread,
                                     jint depth,
                                     jmethodID* method_ptr,
                                     jlocation* location_ptr);

  static jvmtiError GetStackTrace(jvmtiEnv* env,
                                  jthread thread,
                                  jint start_depth,
                                  jint max_frame_count,
                                  jvmtiFrameInfo* frame_buffer,
                                  jint* count_ptr);

  static jvmtiError GetThreadListStackTraces(jvmtiEnv* env,
                                             jint thread_count,
                                             const jthread* thread_list,
                                             jint max_frame_count,
                                             jvmtiStackInfo** stack_info_ptr);
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_STACK_H_
