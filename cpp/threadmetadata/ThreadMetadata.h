// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <jni.h>
#include <sys/types.h>

namespace facebook {
namespace profilo {
namespace threadmetadata {

void logThreadMetadata(JNIEnv*, jobject);

} // threadmetadata
} // profilo
} // facebook
