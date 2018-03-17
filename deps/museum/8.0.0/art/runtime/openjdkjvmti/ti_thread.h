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

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_THREAD_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_THREAD_H_

#include "jni.h"
#include "jvmti.h"

namespace art {
class ArtField;
}

namespace openjdkjvmti {

class EventHandler;

class ThreadUtil {
 public:
  static void Register(EventHandler* event_handler);
  static void Unregister();

  // To be called when it is safe to cache data.
  static void CacheData();

  static jvmtiError GetAllThreads(jvmtiEnv* env, jint* threads_count_ptr, jthread** threads_ptr);

  static jvmtiError GetCurrentThread(jvmtiEnv* env, jthread* thread_ptr);

  static jvmtiError GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr);

  static jvmtiError GetThreadState(jvmtiEnv* env, jthread thread, jint* thread_state_ptr);

  static jvmtiError SetThreadLocalStorage(jvmtiEnv* env, jthread thread, const void* data);
  static jvmtiError GetThreadLocalStorage(jvmtiEnv* env, jthread thread, void** data_ptr);

  static jvmtiError RunAgentThread(jvmtiEnv* env,
                                   jthread thread,
                                   jvmtiStartFunction proc,
                                   const void* arg,
                                   jint priority);

 private:
  static art::ArtField* context_class_loader_;
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_THREAD_H_
