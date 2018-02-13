/*
 * Copyright (c) 2016-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <fbjni/fbjni.h>

namespace facebook {
namespace jni {

// JNI's NIO support has some awkward preconditions and error reporting. This
// class provides much more user-friendly access.
class JByteBuffer : public JavaClass<JByteBuffer> {
 public:
  static constexpr const char* kJavaDescriptor = "Ljava/nio/ByteBuffer;";

  static local_ref<JByteBuffer> wrapBytes(uint8_t* data, size_t size);

  bool isDirect() const;

  uint8_t* getDirectBytes() const;
  size_t getDirectSize() const;
};

}}
