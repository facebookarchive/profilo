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

#include "ProcFsUtils.h"

#include <dirent.h>
#include <stdlib.h>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace facebook {
namespace profilo {
namespace util {

namespace {
static constexpr int kMaxProcFileLength = 64;
}

// Return all the numeric items in the folder passed as parameter.
// Non-numeric items are ignored.
static std::unordered_set<uint32_t> numericFolderItems(const char* folder) {
  DIR* dir = opendir(folder);
  if (dir == nullptr) {
    throw std::system_error(errno, std::system_category());
  }
  std::unordered_set<uint32_t> items;
  dirent* result = nullptr;
  errno = 0;
  while ((result = readdir(dir)) != nullptr) {
    // Skip navigation entries
    if (strcmp(".", result->d_name) == 0 || strcmp("..", result->d_name) == 0) {
      continue;
    }

    errno = 0;
    char* endptr = nullptr;
    uint32_t item = strtoul(result->d_name, &endptr, /*base*/ 10);
    if (errno != 0 || *endptr != '\0') {
      continue; // unable to parse item
    }
    items.emplace(item);
  }
  if (closedir(dir) != 0) {
    throw std::system_error(errno, std::system_category(), "closedir");
  }
  return items;
}

ThreadList threadListFromProcFs() {
  return ThreadList(numericFolderItems("/proc/self/task/"));
}

FdList fdListFromProcFs() {
  return FdList(numericFolderItems("/proc/self/fd/"));
}

PidList pidListFromProcFs() {
  return PidList(numericFolderItems("/proc/"));
}

std::string processName() {
  FILE* cmdFile = fopen("/proc/self/cmdline", "r");
  if (cmdFile == nullptr) {
    return "";
  }
  char procName[255]{};
  char* res = fgets(procName, 255, cmdFile);
  fclose(cmdFile);
  if (res == nullptr) {
    return "";
  }

  return std::string(procName);
}

std::string getThreadName(uint32_t thread_id) {
  char threadNamePath[kMaxProcFileLength]{};
  int bytesWritten = snprintf(
      &threadNamePath[0],
      kMaxProcFileLength,
      "/proc/self/task/%d/comm",
      thread_id);
  if (bytesWritten < 0 || bytesWritten >= kMaxProcFileLength) {
    errno = 0;
    return "";
  }
  FILE* threadNameFile = fopen(threadNamePath, "r");
  if (threadNameFile == nullptr) {
    errno = 0;
    return "";
  }

  char threadName[16]{};
  char* res = fgets(threadName, 16, threadNameFile);
  fclose(threadNameFile);
  errno = 0;
  if (res == nullptr) {
    return "";
  }

  return std::string(threadName);
}

} // namespace util
} // namespace profilo
} // namespace facebook
