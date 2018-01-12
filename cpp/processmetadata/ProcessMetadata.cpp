// Copyright 2004-present Facebook. All Rights Reserved.

#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include <loom/LogEntry.h>
#include <loom/Logger.h>
#include <loom/processmetadata/ProcessMetadata.h>
#include <util/ProcFs.h>
#include <util/common.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace facebook {
namespace loom {
namespace processmetadata {

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

static bool shouldParseFolder(uid_t uid, const char* folder) {
  struct stat folderStat;
  if (stat(folder, &folderStat) < 0) {
    return false;
  }
  return folderStat.st_uid == uid;
}

/*
 * We obtain the process name from procfs. The proc/ has folders with names
 * as PIDs for processes. The process name can be found in cmdline file. This
 * can be empty for some kernel processes. This works on Android 8 too.
 *
 * For early versions of Android, other processes' folders are visible. So, we
 * use UID to identify folders belonging to our app.
 *
 * For a detailled drilldown on proc fs, please refer to
 * http://man7.org/linux/man-pages/man5/proc.5.html.
 */
static void logProcessNames(Logger& logger) {
  DIR* dir;
  struct dirent* ent;
  std::stringstream ss;
  uid_t uid = getuid();
  if ((dir = opendir("/proc")) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      char* subDirName = ent->d_name;
      int pid = atoi(subDirName);
      if (pid == 0 && subDirName[0] != '0') {
        continue; // We are only looking for directories with numbers as names.
      }
      int len = strlen(subDirName) + 7; // length of "/proc/" is 6.
      char procDirName[len];
      memset(procDirName, 0, len);
      int status = snprintf(procDirName, len, "/proc/%s", subDirName);
      if (status < 0 || status >= len) {
        throw std::runtime_error(
            "Unable to parse procfs for PID: " + std::string(subDirName));
      }
      if (shouldParseFolder(uid, procDirName)) {
        std::string line;
        len = strlen(subDirName) + 15; // strlen("/proc//cmdline") is 14.
        char cmdFileName[len];
        memset(cmdFileName, 0, len);
        status = snprintf(cmdFileName, len, "/proc/%s/cmdline", subDirName);
        if (status < 0 || status >= len) {
          throw std::runtime_error(
              "Unable to parse procfs for PID: " + std::string(subDirName));
        }
        std::ifstream cmdlineStream(cmdFileName);
        if (getline(cmdlineStream, line)) {
          ss << line.c_str() << "(" << pid << "),";
        }
        cmdlineStream.close();
      }
    }
    closedir(dir);
  }
  std::string processList = ss.str();
  if (!processList.empty()) {
    processList.erase(
        processList.end() - 1, processList.end()); // Delete , at end.
  }
  logAnnotation(
      logger, entries::PROCESS_LIST, "processes", processList.c_str());
}

void logProcessMetadata(JNIEnv*, jobject) {
  auto& logger = Logger::get();
  logProcessNames(logger);
}

} // namespace processmetadata
} // namespace loom
} // namespace facebook
