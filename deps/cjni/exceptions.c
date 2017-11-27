// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <stdbool.h>
#include <stdio.h>

#include "exceptions.h"

bool throw_exception(
    JNIEnv* env,
    const char* name,
    const char* fmt,
    ...)
{
  if ((*env)->ExceptionCheck(env)) {
    return false;
  }
  jclass exception_class = (*env)->FindClass(env, name);
  if (exception_class == NULL) {
    return false;
  }

  char err_msg[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(err_msg, sizeof(err_msg), fmt, args);
  va_end(args);

  return ((*env)->ThrowNew(env, exception_class, err_msg) == 0);
}
