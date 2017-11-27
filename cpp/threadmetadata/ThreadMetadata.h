// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <jni.h>
#include <sys/types.h>

namespace facebook {
namespace loom {
namespace threadmetadata {

void logThreadMetadata(JNIEnv*, jobject);

} // threadmetadata
} // loom
} // facebook
