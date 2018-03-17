/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_HEAP_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_HEAP_H_

#include "jvmti.h"

namespace openjdkjvmti {

class ObjectTagTable;

class HeapUtil {
 public:
  explicit HeapUtil(ObjectTagTable* tags) : tags_(tags) {
  }

  jvmtiError GetLoadedClasses(jvmtiEnv* env, jint* class_count_ptr, jclass** classes_ptr);

  jvmtiError IterateThroughHeap(jvmtiEnv* env,
                                jint heap_filter,
                                jclass klass,
                                const jvmtiHeapCallbacks* callbacks,
                                const void* user_data);

  jvmtiError FollowReferences(jvmtiEnv* env,
                              jint heap_filter,
                              jclass klass,
                              jobject initial_object,
                              const jvmtiHeapCallbacks* callbacks,
                              const void* user_data);

  static jvmtiError ForceGarbageCollection(jvmtiEnv* env);

  ObjectTagTable* GetTags() {
    return tags_;
  }

  static void Register();
  static void Unregister();

 private:
  ObjectTagTable* tags_;
};

class HeapExtensions {
 public:
  static jvmtiError JNICALL GetObjectHeapId(jvmtiEnv* env, jlong tag, jint* heap_id, ...);
  static jvmtiError JNICALL GetHeapName(jvmtiEnv* env, jint heap_id, char** heap_name, ...);

  static jvmtiError JNICALL IterateThroughHeapExt(jvmtiEnv* env,
                                                  jint heap_filter,
                                                  jclass klass,
                                                  const jvmtiHeapCallbacks* callbacks,
                                                  const void* user_data);
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_HEAP_H_
