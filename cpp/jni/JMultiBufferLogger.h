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

#include <pthread.h>
#include <functional>
#include <memory>
#include <vector>

#include <fbjni/fbjni.h>
#include <profilo/MultiBufferLogger.h>
#include <profilo/mmapbuf/JBuffer.h>

namespace fbjni = facebook::jni;
using namespace facebook::profilo::mmapbuf;

namespace facebook {
namespace profilo {
namespace logger {

class JMultiBufferLogger : public fbjni::HybridClass<JMultiBufferLogger> {
 public:
  static constexpr const char* kJavaDescriptor =
      "Lcom/facebook/profilo/logger/MultiBufferLogger;";

  static void initHybrid(facebook::jni::alias_ref<jhybridobject>);
  static void registerNatives();

  void addBuffer(JBuffer* buffer);
  void removeBuffer(JBuffer* buffer);

  jint writeStandardEntry(
      jint flags,
      jint type,
      jlong timestamp,
      jint tid,
      jint arg1,
      jint arg2,
      jlong arg3);

  jint writeBytesEntry(jint flags, jint type, jint arg1, jstring arg2);

  MultiBufferLogger& nativeInstance();

 private:
  MultiBufferLogger logger_;
};

} // namespace logger
} // namespace profilo
} // namespace facebook
