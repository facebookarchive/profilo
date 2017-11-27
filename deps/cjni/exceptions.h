// Copyright 2004-present Facebook. All Rights Reserved.

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
