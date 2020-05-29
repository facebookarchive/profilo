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

#include <fcntl.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fbjni/fbjni.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/header/MmapBufferHeader.h>

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace header {

namespace {
struct FileDescriptor {
  int fd;

  FileDescriptor(const std::string& path) {
    fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
      throw std::runtime_error("Error while opening a buffer file");
    }
  }

  ~FileDescriptor() {
    close(fd);
  }
};
} // namespace

class MmapBufferHeaderReader
    : public fbjni::HybridClass<MmapBufferHeaderReader> {
 public:
  constexpr static auto kJavaDescriptor =
      "Lcom/facebook/profilo/mmapbuf/reader/MmapBufferHeaderReader;";

  static fbjni::local_ref<MmapBufferHeaderReader::jhybriddata> initHybrid(
      fbjni::alias_ref<jclass>) {
    return makeCxxInstance();
  }

  //
  // Returns trace ID if a trace was active at the time of process death.
  //
  int64_t readTraceId(const std::string& buffer_path) {
    FileDescriptor fd(buffer_path);
    int headerSize = sizeof(MmapBufferPrefix);
    char buf[headerSize];
    if (read(fd.fd, static_cast<void*>(buf), headerSize) != headerSize) {
      return 0;
    }

    MmapBufferPrefix* header = reinterpret_cast<MmapBufferPrefix*>(buf);

    if (header->staticHeader.magic != kMagic) {
      return 0;
    }

    if (header->staticHeader.version != kVersion) {
      return 0;
    }

    if (header->header.bufferVersion != RingBuffer::kVersion) {
      return 0;
    }

    return header->header.traceId;
  }

  static void registerNatives() {
    registerHybrid({
        makeNativeMethod("initHybrid", MmapBufferHeaderReader::initHybrid),
        makeNativeMethod("readTraceId", MmapBufferHeaderReader::readTraceId),
    });
  }
};

} // namespace header
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  return fbjni::initialize(vm, [] {
    facebook::profilo::mmapbuf::header::MmapBufferHeaderReader::
        registerNatives();
  });
}
