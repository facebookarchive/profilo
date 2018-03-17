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

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_H_

#include "jni.h"
#include "jvmti.h"

namespace openjdkjvmti {

class EventHandler;

class ClassUtil {
 public:
  static void Register(EventHandler* event_handler);
  static void Unregister();

  static jvmtiError GetClassFields(jvmtiEnv* env,
                                   jclass klass,
                                   jint* field_count_ptr,
                                   jfieldID** fields_ptr);

  static jvmtiError GetClassMethods(jvmtiEnv* env,
                                    jclass klass,
                                    jint* method_count_ptr,
                                    jmethodID** methods_ptr);

  static jvmtiError GetImplementedInterfaces(jvmtiEnv* env,
                                             jclass klass,
                                             jint* interface_count_ptr,
                                             jclass** interfaces_ptr);

  static jvmtiError GetClassModifiers(jvmtiEnv* env, jclass klass, jint* modifiers_ptr);

  static jvmtiError GetClassSignature(jvmtiEnv* env,
                                      jclass klass,
                                      char** signature_ptr,
                                      char** generic_ptr);

  static jvmtiError GetClassStatus(jvmtiEnv* env, jclass klass, jint* status_ptr);

  static jvmtiError GetClassLoader(jvmtiEnv* env, jclass klass, jobject* classloader_ptr);

  static jvmtiError GetClassLoaderClasses(jvmtiEnv* env,
                                          jobject initiating_loader,
                                          jint* class_count_ptr,
                                          jclass** classes_ptr);

  static jvmtiError IsInterface(jvmtiEnv* env, jclass klass, jboolean* is_interface_ptr);
  static jvmtiError IsArrayClass(jvmtiEnv* env, jclass klass, jboolean* is_array_class_ptr);

  static jvmtiError GetClassVersionNumbers(jvmtiEnv* env,
                                           jclass klass,
                                           jint* minor_version_ptr,
                                           jint* major_version_ptr);
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_CLASS_H_
