// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <jni.h>
#include <unistd.h>

namespace facebook {
namespace art {

struct JavaFrame {
  uint32_t methodIdx;
  uint32_t dexSignature;
};

void InitRuntime();
void InstallRuntime(JNIEnv* env, void* thread);
size_t GetStackTrace(JavaFrame* frames, size_t max_frames, void* thread);

} // namespace art601
} // namespace facebook
