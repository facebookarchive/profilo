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
#include <fstream>
#include <memory>
#include <stdexcept>

#include <profilo/entries/EntryType.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <profilo/mmapbuf/Buffer.h>
#include <profilo/mmapbuf/header/MmapBufferHeader.h>
#include <profilo/util/common.h>
#include <profilo/writer/TraceWriter.h>
#include <profilo/writer/trace_headers.h>

namespace facebook {
namespace profilo {
namespace mmapbuf {
namespace writer {

using namespace facebook::profilo::mmapbuf::header;

namespace {

constexpr int64_t kTriggerEventFlag = 0x0002000000000000L; // 1 << 49
static constexpr char kMemoryMappingKey[] = "l:s:u:o:s";

void loggerWrite(
    Logger& logger,
    EntryType type,
    int32_t callid,
    int32_t matchid,
    int64_t extra,
    int64_t timestamp = monotonicTime()) {
  logger.write(StandardEntry{
      .id = 0,
      .type = type,
      .timestamp = timestamp,
      .tid = threadID(),
      .callid = callid,
      .matchid = matchid,
      .extra = extra,
  });
}

void loggerWriteStringAnnotation(
    Logger& logger,
    EntryType type,
    int32_t callid,
    const std::string& annotationKey,
    const std::string& annotationValue,
    int64_t extra = 0,
    int64_t timestamp = monotonicTime()) {
  StandardEntry annotationEntry{};
  annotationEntry.type = type;
  annotationEntry.tid = threadID();
  annotationEntry.timestamp = timestamp;
  annotationEntry.callid = callid;
  auto matchid = logger.write(std::move(annotationEntry));

  auto key = annotationKey.c_str();
  matchid = logger.writeBytes(
      EntryType::STRING_KEY,
      matchid,
      reinterpret_cast<const uint8_t*>(key),
      strlen(key));
  auto value = annotationValue.c_str();
  logger.writeBytes(
      EntryType::STRING_VALUE,
      matchid,
      reinterpret_cast<const uint8_t*>(value),
      strlen(value));
}

void loggerWriteTraceStringAnnotation(
    Logger& logger,
    int32_t annotationQuicklogId,
    const std::string& annotationKey,
    const std::string& annotationValue,
    int64_t timestamp = monotonicTime()) {
  loggerWriteStringAnnotation(
      logger,
      EntryType::TRACE_ANNOTATION,
      annotationQuicklogId,
      annotationKey,
      annotationValue,
      0,
      timestamp);
}

void loggerWriteQplTriggerAnnotation(
    Logger& logger,
    int32_t marker_id,
    const std::string& annotationKey,
    const std::string& annotationValue,
    int64_t timestamp = monotonicTime()) {
  loggerWriteStringAnnotation(
      logger,
      EntryType::QPL_ANNOTATION,
      marker_id,
      annotationKey,
      annotationValue,
      kTriggerEventFlag,
      timestamp);
}

//
// Process entries from source buffer and write to the destination.
// It's okay if not all entries were successfully copied.
// Returns false if were able to copy less than 50% of source buffer entries.
//
bool copyBufferEntries(TraceBuffer& source, TraceBuffer& dest) {
  TraceBuffer::Cursor cursor = source.currentTail(0);
  alignas(4) Packet packet;
  uint32_t processed_count = 0;
  while (source.tryRead(packet, cursor)) {
    Packet writePacket = packet; // copy
    dest.write(writePacket);
    ++processed_count;
    if (!cursor.moveForward()) {
      break;
    }
  }
  return processed_count > 0;
}

void processMemoryMappingsFile(
    Logger& logger,
    std::string& file_path,
    int64_t timestamp) {
  std::ifstream mappingsFile(file_path);
  if (!mappingsFile.is_open()) {
    return;
  }

  int32_t tid = threadID();
  std::string mappingLine;
  while (std::getline(mappingsFile, mappingLine)) {
    auto mappingId = logger.write(entries::StandardEntry{
        .type = EntryType::MAPPING,
        .timestamp = timestamp,
        .tid = tid,
    });
    auto keyId = logger.writeBytes(
        EntryType::STRING_KEY,
        mappingId,
        reinterpret_cast<const uint8_t*>(kMemoryMappingKey),
        sizeof(kMemoryMappingKey));
    logger.writeBytes(
        EntryType::STRING_VALUE,
        keyId,
        reinterpret_cast<const uint8_t*>(mappingLine.c_str()),
        mappingLine.size());
  }
}

} // namespace

int64_t MmapBufferTraceWriter::nativeInitAndVerify(
    const std::string& dump_path) {
  dump_path_ = dump_path;
  bufferMapHolder_ = std::make_unique<BufferFileMapHolder>(dump_path);

  MmapBufferPrefix* mapBufferPrefix =
      reinterpret_cast<MmapBufferPrefix*>(bufferMapHolder_->map_ptr);
  if (mapBufferPrefix->staticHeader.magic != kMagic) {
    return 0;
  }

  if (mapBufferPrefix->staticHeader.version != kVersion) {
    return 0;
  }

  if (mapBufferPrefix->header.bufferVersion != RingBuffer::kVersion) {
    return 0;
  }

  int64_t trace_id = mapBufferPrefix->header.traceId;
  trace_id_ = trace_id;
  return trace_id;
}

void MmapBufferTraceWriter::nativeWriteTrace(
    const std::string& type,
    const std::string& trace_folder,
    const std::string& trace_prefix,
    int32_t trace_flags,
    fbjni::alias_ref<JNativeTraceWriterCallbacks> callbacks) {
  writeTrace(
      type,
      trace_folder,
      trace_prefix,
      trace_flags,
      std::make_shared<NativeTraceWriterCallbacksProxy>(callbacks));
}

void MmapBufferTraceWriter::writeTrace(
    const std::string& type,
    const std::string& trace_folder,
    const std::string& trace_prefix,
    int32_t trace_flags,
    std::shared_ptr<TraceCallbacks> callbacks,
    uint64_t timestamp) {
  if (bufferMapHolder_.get() == nullptr) {
    throw std::runtime_error(
        "Not initialized. Method nativeInitAndVerify() should be called first.");
  }
  if (trace_id_ == 0) {
    throw std::runtime_error(
        "Buffer is not associated with a trace. Trace Id is 0.");
  }

  MmapBufferPrefix* mapBufferPrefix =
      reinterpret_cast<MmapBufferPrefix*>(bufferMapHolder_->map_ptr);
  int32_t qpl_marker_id =
      static_cast<int32_t>(mapBufferPrefix->header.longContext);

  auto entriesCount = mapBufferPrefix->header.size;
  // Number of additional records we need to log in addition to entries from the
  // buffer file + memory mappings file records + some buffer for long string
  // entries.
  constexpr auto kExtraRecordCount = 4096;

  std::shared_ptr<mmapbuf::Buffer> buffer =
      std::make_shared<mmapbuf::Buffer>(entriesCount + kExtraRecordCount);
  auto& ringBuffer = buffer->ringBuffer();
  TraceBuffer::Cursor startCursor = ringBuffer.currentHead();
  Logger::EntryIDCounter newBufferEntryID{1};
  Logger logger([&]() -> TraceBuffer& { return ringBuffer; }, newBufferEntryID);

  // It's not technically backwards trace but that's what we use to denote Black
  // Box traces.
  loggerWrite(
      logger, EntryType::TRACE_BACKWARDS, 0, trace_flags, trace_id_, timestamp);

  {
    // Copying entries from the saved buffer to the new one.
    TraceBuffer* historicBuffer = reinterpret_cast<TraceBuffer*>(
        reinterpret_cast<char*>(bufferMapHolder_->map_ptr) +
        sizeof(MmapBufferPrefix));
    bool ok = copyBufferEntries(*historicBuffer, ringBuffer);
    if (!ok) {
      throw std::runtime_error("Unable to read the file-backed buffer.");
    }
  }

  loggerWrite(
      logger,
      EntryType::QPL_START,
      qpl_marker_id,
      0,
      kTriggerEventFlag,
      timestamp);
  loggerWrite(
      logger,
      EntryType::TRACE_ANNOTATION,
      QuickLogConstants::APP_VERSION_CODE,
      0,
      mapBufferPrefix->header.versionCode,
      timestamp);
  loggerWrite(
      logger,
      EntryType::TRACE_ANNOTATION,
      QuickLogConstants::CONFIG_ID,
      0,
      mapBufferPrefix->header.configId,
      timestamp);
  loggerWriteTraceStringAnnotation(
      logger,
      QuickLogConstants::SESSION_ID,
      "Asl Session Id",
      std::string(mapBufferPrefix->header.sessionId));
  loggerWriteQplTriggerAnnotation(
      logger, qpl_marker_id, "type", type, timestamp);
  loggerWriteQplTriggerAnnotation(
      logger, qpl_marker_id, "collection_method", "persistent", timestamp);

  const char* mapsFilename = mapBufferPrefix->header.memoryMapsFilename;
  if (mapsFilename[0] != '\0') {
    auto lastSlashIdx = dump_path_.rfind("/");
    std::string mapsPath =
        dump_path_.substr(0, lastSlashIdx + 1) + mapsFilename;
    processMemoryMappingsFile(logger, mapsPath, timestamp);
  }

  loggerWrite(logger, EntryType::TRACE_END, 0, 0, trace_id_, timestamp);

  TraceWriter writer(
      std::move(trace_folder),
      std::move(trace_prefix),
      buffer,
      callbacks,
      calculateHeaders(mapBufferPrefix->header.pid));

  try {
    writer.processTrace(trace_id_, startCursor);
  } catch (std::exception& e) {
    FBLOGE("Error during dump processing: %s", e.what());
    callbacks->onTraceAbort(trace_id_, AbortReason::UNKNOWN);
  }
}

fbjni::local_ref<MmapBufferTraceWriter::jhybriddata>
MmapBufferTraceWriter::initHybrid(fbjni::alias_ref<jclass>) {
  return makeCxxInstance();
}

void MmapBufferTraceWriter::registerNatives() {
  registerHybrid({
      makeNativeMethod("initHybrid", MmapBufferTraceWriter::initHybrid),
      makeNativeMethod(
          "nativeWriteTrace", MmapBufferTraceWriter::nativeWriteTrace),
      makeNativeMethod(
          "nativeInitAndVerify", MmapBufferTraceWriter::nativeInitAndVerify),
  });
}

} // namespace writer
} // namespace mmapbuf
} // namespace profilo
} // namespace facebook
