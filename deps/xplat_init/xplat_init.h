// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <jni.h>

namespace facebook { namespace xplat {

__attribute__((visibility("default")))
jint initialize(JavaVM* vm, void(*init_fn)()) noexcept;

}}
