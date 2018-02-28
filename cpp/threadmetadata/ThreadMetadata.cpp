// Copyright 2004-present Facebook. All Rights Reserved.

#include <cstring>

#include <profilo/threadmetadata/ThreadMetadata.h>
#include <profilo/Logger.h>
#include <profilo/LogEntry.h>
#include <util/common.h>
#include <util/ProcFs.h>

#include <sys/resource.h>

namespace facebook {
namespace profilo {
namespace threadmetadata {

static int32_t logAnnotation(
  Logger& logger,
  entries::EntryType type,
  const char* key,
  const char* value) {

  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = type;

  int32_t matchId = logger.write(std::move(entry));
  if (key != nullptr) {
    matchId = logger.writeBytes(
      entries::STRING_KEY,
      matchId,
      reinterpret_cast<const uint8_t*>(key),
      strlen(key));
  }
  return logger.writeBytes(
    entries::STRING_VALUE,
    matchId,
    reinterpret_cast<const uint8_t*>(value),
    strlen(value));
}

static void logThreadName(Logger& logger, uint32_t tid) {
  std::string threadName = util::getThreadName(tid);

  if (threadName.empty()) {
    return;
  }

  char threadId[16]{};
  snprintf(threadId, 16, "%d", tid);

  logAnnotation(
    logger,
    entries::TRACE_THREAD_NAME,
    threadId,
    threadName.data());
}

static void logThreadPriority(Logger& logger, int32_t tid) {
  errno = 0;
  int priority = getpriority(PRIO_PROCESS, tid);
  if (priority == -1 && errno != 0) {
    errno = 0;
    return; // Priority is not available
  }

  logger.write(StandardEntry {
    .id = 0,
    .type = entries::TRACE_THREAD_PRI,
    .timestamp = monotonicTime(),
    .tid = tid,
    .callid = 0,
    .matchid = 0,
    .extra = priority,
  });
}

/* Log thread names and priorities. */
void logThreadMetadata(JNIEnv*, jobject) {
  const auto& threads = util::threadListFromProcFs();
  auto& logger = Logger::get();

  for (auto& tid : threads) {
    logThreadName(logger, tid);
    logThreadPriority(logger, tid);
  }
}

} // threadmetadata
} // profilo
} // facebook
