/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <jni.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool throw_exception(JNIEnv* env, const char* name, const char* fmt, ...);

#define throw_runtime_exception(env, ...) \
  throw_exception((env), "java/lang/RuntimeException", __VA_ARGS__)

#define throw_illegal_argument_exception(env, ...) \
  throw_exception((env), "java/lang/IllegalArgumentException", __VA_ARGS__)

#define throw_out_of_memory_error(env, ...) \
  throw_exception((env), "java/lang/OutOfMemoryError", __VA_ARGS__)

#ifdef __cplusplus
}
#endif
