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

#include <atomic>
#include <dlfcn.h>
#include <stdarg.h>
#include <system_error>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_set>
#include <sstream>
#include <libgen.h>

#include <fb/Build.h>
#include <fbjni/fbjni.h>
#include <fb/log.h>
#include <fb/xplat_init.h>
#include <linker/linker.h>
#include <util/hooks.h>

#include <profilo/Logger.h>
#include <profilo/LogEntry.h>
#include <profilo/TraceProviders.h>

#include <unordered_set>

#include "Atrace.h"

namespace facebook {
namespace profilo {
namespace atrace {

namespace {

int *atrace_marker_fd = nullptr;
std::atomic<uint64_t> *atrace_enabled_tags = nullptr;
std::atomic<uint64_t> original_tags(UINT64_MAX);
std::atomic<bool> systrace_installed;
std::atomic<uint32_t> provider_mask;
bool first_enable = true;

namespace {

ssize_t const kAtraceMessageLength = 1024;

void log_systrace(int fd, const void *buf, size_t count) {
  if (systrace_installed &&
      fd == *atrace_marker_fd &&
      count > 0 &&
      TraceProviders::get().isEnabled(provider_mask.load())) {

    const char *msg = reinterpret_cast<const char *>(buf);

    EntryType type;
    switch (msg[0]) {
      case 'B': { // begin synchronous event. format: "B|<pid>|<name>"
        type = entries::MARK_PUSH;
        break;
      }
      case 'E': { // end synchronous event. format: "E"
        type = entries::MARK_POP;
        break;
      }
      // the following events we don't currently log.
      case 'S': // start async event. format: "S|<pid>|<name>|<cookie>"
      case 'F': // finish async event. format: "F|<pid>|<name>|<cookie>"
      case 'C': // counter. format: "C|<pid>|<name>|<value>"
      default:
        return;
    }

    auto& logger = Logger::get();
    StandardEntry entry{};
    entry.tid = threadID();
    entry.timestamp = monotonicTime();
    entry.type = type;

    int32_t id = logger.write(std::move(entry));
    if (type != entries::MARK_POP) {
      // Format is B|<pid>|<name>.
      // Skip "B|" trivially, find next '|' with memchr. We cannot use strchr
      // since we can't trust the message to have a null terminator.
      constexpr auto kPrefixLength = 2; //length of "B|";

      const char* name = reinterpret_cast<const char*>(
        memchr(msg + kPrefixLength, '|', count - kPrefixLength));
      if (name == nullptr) {
        return;
      }
      name++; // skip '|' to the next character
      ssize_t len = msg + count - name;
      if (len > 0) {
        id = logger.writeBytes(
          entries::STRING_KEY,
          id,
          (const uint8_t *)"__name",
          6);
        logger.writeBytes(
          entries::STRING_VALUE,
          id,
          (const uint8_t *)name,
          std::min(len, kAtraceMessageLength));

        FBLOGV("systrace event: %s", name);
      }
    }
  }
}

ssize_t write_hook(int fd, const void *buf, size_t count) {
  log_systrace(fd, buf, count);
  return CALL_PREV(write_hook, fd, buf, count);
}

ssize_t __write_chk_hook(int fd, const void *buf, size_t count, size_t buf_size) {
  log_systrace(fd, buf, count);
  return CALL_PREV(__write_chk_hook, fd, buf, count, buf_size);
}

} // namespace

std::vector<std::pair<char const*, void*>>& getFunctionHooks() {
  static std::vector<std::pair<char const*, void*>> functionHooks = {
    {"write", reinterpret_cast<void*>(&write_hook)},
    {"__write_chk", reinterpret_cast<void*>(__write_chk_hook)},
  };
  return functionHooks;
}

// Returns the set of libraries that we don't want to hook.
std::unordered_set<std::string>& getSeenLibs() {
  static bool init = false;
  static std::unordered_set<std::string> seenLibs;

  // Add this library's name to the set that we won't hook
  if (!init) {

    seenLibs.insert("libc.so");

    Dl_info info;
    if (!dladdr((void *)&getSeenLibs, &info)) {
      FBLOGV("Failed to find module name");
    }
    if (info.dli_fname == nullptr) {
      // Not safe to continue as a thread may block trying to hook the current
      // library
      throw std::runtime_error("could not resolve current library");
    }

    seenLibs.insert(basename(info.dli_fname));
    init = true;
  }
  return seenLibs;
}

void hookLoadedLibs() {
  auto& functionHooks = getFunctionHooks();
  auto& seenLibs = getSeenLibs();

  facebook::profilo::hooks::hookLoadedLibs(functionHooks, seenLibs);
}

void installSystraceSnooper(int providerMask) {
  auto sdk = build::Build::getAndroidSdk();
  {
    std::string lib_name("libcutils.so");
    std::string enabled_tags_sym("atrace_enabled_tags");
    std::string fd_sym("atrace_marker_fd");

    if (sdk < 18) {
      lib_name = "libutils.so";
      // android::Tracer::sEnabledTags
      enabled_tags_sym = "_ZN7android6Tracer12sEnabledTagsE";
      // android::Tracer::sTraceFD
      fd_sym = "_ZN7android6Tracer8sTraceFDE";
    }

    void* handle;
    if (sdk < 21) {
      handle = dlopen(lib_name.c_str(), RTLD_LOCAL);
    } else {
      handle = dlopen(nullptr, RTLD_GLOBAL);
    }

    atrace_enabled_tags =
      reinterpret_cast<std::atomic<uint64_t> *>(
          dlsym(handle, enabled_tags_sym.c_str()));

    if (atrace_enabled_tags == nullptr) {
      throw std::runtime_error("Enabled Tags not defined");
    }

    atrace_marker_fd =
      reinterpret_cast<int*>(dlsym(handle, fd_sym.c_str()));

    if (atrace_marker_fd == nullptr) {
      throw std::runtime_error("Trace FD not defined");
    }
    if (*atrace_marker_fd == -1) {
      throw std::runtime_error("Trace FD not valid");
    }
  }

  if (linker_initialize()) {
    throw std::runtime_error("Could not initialize linker library");
  }

  hookLoadedLibs();

  systrace_installed = true;
  provider_mask = providerMask;
}

void enableSystrace() {
  if (!systrace_installed) {
    return;
  }

  if (!first_enable) {
    // On every enable, except the first one, find if new libs were loaded
    // and install systrace hook for them
    try {
      hookLoadedLibs();
    } catch (...) {
      // It's ok to continue if the refresh has failed
    }
  }
  first_enable = false;

  auto prev = atrace_enabled_tags->exchange(UINT64_MAX);
  if (prev != UINT64_MAX) { // if we somehow call this twice in a row, don't overwrite the real tags
    original_tags = prev;
  }
}

void restoreSystrace() {
  if (!systrace_installed) {
    return;
  }

  uint64_t tags = original_tags;
  if (tags != UINT64_MAX) { // if we somehow call this before enableSystrace, don't screw it up
    atrace_enabled_tags->store(tags);
  }
}

bool installSystraceHook(int mask) {
  try {
    installSystraceSnooper(mask);

    return true;
  } catch (const std::runtime_error& e) {
    FBLOGW("could not install hooks: %s", e.what());
    return false;
  }
}

} // anonymous

bool JNI_installSystraceHook(JNIEnv*, jobject, jint mask) {
  return installSystraceHook(mask);
}

void JNI_enableSystraceNative(JNIEnv*, jobject) {
  enableSystrace();
}

void JNI_restoreSystraceNative(JNIEnv*, jobject) {
  restoreSystrace();
}

namespace fbjni = facebook::jni;

void registerNatives() {
  fbjni::registerNatives(
    "com/facebook/profilo/provider/atrace/Atrace",
    {
      makeNativeMethod("installSystraceHook", "(I)Z", JNI_installSystraceHook),
      makeNativeMethod("enableSystraceNative", "()V", JNI_enableSystraceNative),
      makeNativeMethod("restoreSystraceNative", "()V", JNI_restoreSystraceNative),
    });
}

} // atrace
} // profilo
} // facebook
