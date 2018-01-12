// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <jni.h>
#include <sys/types.h>

namespace facebook {
namespace loom {
namespace processmetadata {

void logProcessMetadata(JNIEnv*, jobject);

} // namespace processmetadata
} // namespace loom
} // namespace facebook
