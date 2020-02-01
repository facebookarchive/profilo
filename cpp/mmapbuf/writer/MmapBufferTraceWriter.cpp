/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain a
 * copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include "MmapBufferTraceWriter.h"

#include <fb/fbjni.h>
#include <fb/log.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <stdexcept>

#include <profilo/entries/EntryType.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/MmapBufferPrefix.h>
#include <profilo/writer/TraceWriter.h>
#include <profilo/writer/trace_headers.h>
#include <util/common.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace writer {

namespace {

constexpr int64_t kTriggerEventFlag = 0x0002000000000000L; // 1 << 49

void loggerWrite(
    Logger& logger,
    EntryType type,
    int32_t callid,
    int64_t extra,
    int64_t timestamp = monotonicTime()) {
  logger.write(StandardEntry{
      .id = 0,
      .type = static_cast<decltype(StandardEntry::type)>(type),
      .timestamp = timestamp,
      .tid = threadID(),
      .callid = callid,
      .matchid = 0,
      .extra = extra,
  });
}

void loggerWriteStringAnnotation(
    Logger& logger,
    int32_t annotationQuicklogId,
    std::string annotationKey,
    std::string annotationValue,
    int64_t timestamp = monotonicTime()) {
  StandardEntry annotationEntry{};
  annotationEntry.type = entries::TRACE_ANNOTATION;
  annotationEntry.tid = threadID();
  annotationEntry.timestamp = timestamp;
  annotationEntry.callid = annotationQuicklogId;
  auto matchid = logger.write(std::move(annotationEntry));

  auto key = annotationKey.c_str();
  matchid = logger.writeBytes(
      entries::STRING_KEY,
      matchid,
      reinterpret_cast<const uint8_t*>(key),
      strlen(key));
  auto value = annotationValue.c_str();
  logger.writeBytes(
      entries::STRING_VALUE,
      matchid,
      reinterpret_cast<const uint8_t*>(value),
      strlen(value));
}

struct FileDescriptor {
  int fd;

  FileDescriptor(const std::string& dump_path) {
    fd = open(dump_path.c_str(), O_RDONLY);
    if (fd == -1) {
      FBLOGE(
          "Unable to open a dump file %s, errno: %s",
          dump_path.c_str(),
          std::strerror(errno));
      throw std::runtime_error("Error while opening a dump file");
    }
  }

  ~FileDescriptor() {
    close(fd);
  }
};

struct BufferFileMapHolder {
  void* map_ptr;
  size_t size;

  BufferFileMapHolder(const std::string& dump_path) {
    FileDescriptor fd(dump_path);
    struct stat fileStat;
    int res = fstat(fd.fd, &fileStat);
    if (res != 0) {
      throw std::runtime_error("Unable to read fstat from the buffer file");
    }
    size = (size_t)fileStat.st_size;
    if (size == 0) {
      throw std::runtime_error("Empty buffer file");
    }
    map_ptr = mmap(nullptr, (size_t)size, PROT_READ, MAP_PRIVATE, fd.fd, 0);
    if (map_ptr == MAP_FAILED) {
      throw std::runtime_error("Failed to map the buffer file");
    }
  }

  ~BufferFileMapHolder() {
    munmap(map_ptr, size);
  }
};

//
// Process entries from source buffer and write to the destination.
// It's okay if not all entries were successfully copied.
// Returns false if were able to copy less than 50% of source buffer entries.
//
bool copyBufferEntries(TraceBuffer& source, TraceBuffer& dest) {
  TraceBuffer::Cursor cursor = source.currentTail(0);
  alignas(4) Packet packet;
  uint32_t processed_count = 0;
  auto capacity = source.capacity();
  while (source.tryRead(packet, cursor)) {
    Packet writePacket = packet; // copy
    dest.write(writePacket);
    ++processed_count;
    if (!cursor.moveForward()) {
      break;
    }
  }
  return processed_count > capacity / 2;
}
} // namespace

MmapBufferTraceWriter::MmapBufferTraceWriter(
    std::string trace_folder,
    std::string trace_prefix,
    std::shared_ptr<TraceCallbacks> callbacks)
    : trace_folder_(trace_folder),
      trace_prefix_(trace_prefix),
      callbacks_(callbacks) {}

void MmapBufferTraceWriter::nativeWriteTrace(
    const std::string& dump_path,
    int32_t qpl_marker_id) {
  writeTrace(dump_path, qpl_marker_id);
}

void MmapBufferTraceWriter::writeTrace(
    const std::string& dump_path,
    int32_t qpl_marker_id,
    uint64_t timestamp) {
  BufferFileMapHolder bufferFileMap(dump_path);
  MmapBufferPrefix* mapBufferPrefix =
      reinterpret_cast<MmapBufferPrefix*>(bufferFileMap.map_ptr);
  if (mapBufferPrefix->staticHeader.magic != kMagic) {
    return;
  }

  if (mapBufferPrefix->staticHeader.version != kVersion) {
    return;
  }

  if (mapBufferPrefix->header.bufferVersion != RingBuffer::kVersion) {
    return;
  }

  // No trace was active when process died so the buffer is not useful.
  if (mapBufferPrefix->header.normalTraceId == 0 &&
      mapBufferPrefix->header.inMemoryTraceId == 0) {
    return;
  }

  int64_t trace_id = mapBufferPrefix->header.normalTraceId;
  if (mapBufferPrefix->header.inMemoryTraceId != 0) {
    trace_id = mapBufferPrefix->header.inMemoryTraceId;
  }

  auto entriesCount = mapBufferPrefix->header.size;
  // Number of additional records we need to log in addition to entries from the buffer file.
  constexpr auto kServiceRecordCount = 8;

  TraceBufferHolder bufferHolder =
      TraceBuffer::allocate(entriesCount + kServiceRecordCount);
  TraceBuffer::Cursor startCursor = bufferHolder->currentHead();
  Logger logger(
      [&bufferHolder]() -> logger::PacketBuffer& { return *bufferHolder; });

  // It's not technically backwards trace but that's what we use to denote Black
  // Box traces.
  loggerWrite(logger, entries::TRACE_BACKWARDS, 0, trace_id, timestamp);

  {
    // Copying entries from the saved buffer to the new one.
    TraceBuffer* historicBuffer = reinterpret_cast<TraceBuffer*>(
        reinterpret_cast<char*>(bufferFileMap.map_ptr) +
        sizeof(MmapBufferPrefix));
    bool ok = copyBufferEntries(*historicBuffer, *bufferHolder);
    if (!ok) {
      throw std::runtime_error(
          "Unable to read the file-backed buffer. More than 50% of data is corrupted");
    }
  }

  loggerWrite(
      logger, entries::QPL_START, qpl_marker_id, kTriggerEventFlag, timestamp);
  loggerWrite(
      logger,
      entries::TRACE_ANNOTATION,
      QuickLogConstants::APP_VERSION_CODE,
      mapBufferPrefix->header.versionCode,
      timestamp);
  loggerWrite(
      logger,
      entries::TRACE_ANNOTATION,
      QuickLogConstants::CONFIG_ID,
      mapBufferPrefix->header.configId,
      timestamp);
  loggerWriteStringAnnotation(
      logger,
      QuickLogConstants::SESSION_ID,
      "Asl Session Id",
      std::string(mapBufferPrefix->header.sessionId));
  loggerWrite(logger, entries::TRACE_END, 0, trace_id, timestamp);

  TraceWriter writer(
      std::move(trace_folder_),
      std::move(trace_prefix_),
      *bufferHolder,
      callbacks_,
      calculateHeaders());

  try {
    writer.processTrace(startCursor);
  } catch (std::exception& e) {
    FBLOGE("Error during dump processing: %s", e.what());
    callbacks_->onTraceAbort(trace_id, AbortReason::UNKNOWN);
  }
}

fbjni::local_ref<MmapBufferTraceWriter::jhybriddata>
MmapBufferTraceWriter::initHybrid(
    fbjni::alias_ref<jclass>,
    std::string trace_folder,
    std::string trace_prefix,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks) {
  return makeCxxInstance(
      trace_folder,
      trace_prefix,
      std::make_shared<NativeTraceWriterCallbacksProxy>(callbacks));
}

void MmapBufferTraceWriter::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", MmapBufferTraceWriter::initHybrid),
      makeNativeMethod(
          "nativeWriteTrace", MmapBufferTraceWriter::nativeWriteTrace),
  });
}

} // namespace writer
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
