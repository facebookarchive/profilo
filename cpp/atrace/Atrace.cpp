// Copyright 2004-present Facebook. All Rights Reserved.

#include <atomic>
#include <dlfcn.h>
#include <stdarg.h>
#include <system_error>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_set>

#include <fb/Build.h>
#include <fbjni/fbjni.h>
#include <fb/log.h>
#include <fb/xplat_init.h>
#include <linker/linker.h>
#include <linker/bionic_linker.h>

#include <loom/Logger.h>
#include <loom/LogEntry.h>
#include <loom/TraceProviders.h>

#include <unordered_set>

#include "Atrace.h"

namespace facebook {
namespace loom {
namespace atrace {

namespace {

namespace {
struct SharedLibrary {
  SharedLibrary(std::string& lib, int mode):
    handle(dlopen(lib.c_str(), mode)) {}

  ~SharedLibrary() {
    if (handle != nullptr) {
      dlclose(handle);
    }
  }

  void* handle;
};
} // namespace


int *atrace_marker_fd = nullptr;
std::atomic<uint64_t> *atrace_enabled_tags = nullptr;
std::atomic<uint64_t> original_tags(UINT64_MAX);
std::atomic<bool> systrace_installed;
bool first_enable = true;

namespace {

void log_systrace(int fd, const void *buf, size_t count) {
  if (systrace_installed &&
      fd == *atrace_marker_fd &&
      count > 0 &&
      TraceProviders::get().isEnabled(PROVIDER_OTHER)) {

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
      size_t len = msg + count - name;
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
          len);

        FBLOGV("systrace event: %s", name);
      }
    }
  }
}

ssize_t write_hook(int fd, const void *buf, size_t count) {
  log_systrace(fd, buf, count);
  return CALL_PREV(&write, fd, buf, count);
}

inline bool ends_with(std::string const &value, std::string const &ending) {
  if (ending.size() > value.size()) {
    return false;
  }
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

} // namespace

void hookLoadedLibs() {
  static std::unordered_set<std::string> seen_libs;
  Dl_info info;
  if (!dladdr((void *)&hookLoadedLibs, &info)) {
    FBLOGV("Failed to find module name");
  }
  if (info.dli_fname == nullptr) {
    // It's not safe to continue as thread may block trying to hook the current library
    throw std::runtime_error("could not resolve current library");
  }
  std::string cur_module(info.dli_fname);

  FBLOGV("hooking write(2) for loom systrace collection");
  // Reading somain soinfo structure which will allow walking through other
  // global libs
  soinfo* si = reinterpret_cast<soinfo*>(dlopen(nullptr, RTLD_LOCAL));
  for (; si != nullptr; si = si->next) {
    if (!si->link_map_head.l_name) {
      continue;
    }
    std::string libname(si->link_map_head.l_name);

    if (seen_libs.find(libname) != seen_libs.end()) {
      FBLOGV("Found library %s in seen_libs - ignoring", libname.c_str());
      continue;
    }
    FBLOGV("Processing library %s", libname.c_str());
    seen_libs.insert(libname);
    try {
      // Filtering out libs we shouldn't hook
      if (!ends_with(libname, ".so") || ends_with(libname, "libc.so")) {
        continue;
      }
      if (ends_with(cur_module, libname)) {
        continue;
      }

      int res =
          hook_plt_method(si, libname.c_str(), "write", reinterpret_cast<void*>(&write_hook));
      if (res) {
        FBLOGV("could not hook write(2) in %s", libname.c_str());
        continue;
      }
    } catch (const std::runtime_error& e) {
      FBLOGW("Error hooking %s: %s", libname.c_str(), e.what());
      continue;
    }
  }
}

void installSystraceSnooper() {
  auto sdk = build::Build::getAndroidSdk();
  if (sdk > 23 /*Marshmallow*/) {
    FBLOGI("skipping installSystraceSnooper for sdk %i", sdk);
    return;
  }

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

    SharedLibrary libcutils(lib_name, RTLD_LOCAL);

    atrace_enabled_tags =
      reinterpret_cast<std::atomic<uint64_t> *>(
        dlsym(libcutils.handle, enabled_tags_sym.c_str()));
    if (atrace_enabled_tags == nullptr) {
      throw std::runtime_error("Enabled Tags not defined");
    }

    atrace_marker_fd =
      reinterpret_cast<int *>(dlsym(libcutils.handle, fd_sym.c_str()));

    if (atrace_marker_fd == nullptr) {
      throw std::runtime_error("Trace FD not defined");
    }
    if (*atrace_marker_fd == -1) {
      throw std::runtime_error("Trace FD not valid");
    }
  }

  if (linker_initialize()) {
    throw std::runtime_error("could not initialize linker library");
  }

  hookLoadedLibs();

  systrace_installed = true;
  FBLOGI("write(atrace_marker_fd) hook installed");
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

bool installSystraceHook() {
  try {
    installSystraceSnooper();

    return true;
  } catch (const std::runtime_error& e) {
    FBLOGW("could not install hooks: %s", e.what());
    return false;
  }
}

} // anonymous

bool JNI_installSystraceHook(JNIEnv*, jobject) {
  return installSystraceHook();
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
    "com/facebook/loom/provider/atrace/Atrace",
    {
      makeNativeMethod("installSystraceHook", "()Z", JNI_installSystraceHook),
      makeNativeMethod("enableSystraceNative", "()V", JNI_enableSystraceNative),
      makeNativeMethod("restoreSystraceNative", "()V", JNI_restoreSystraceNative),
    });
}

} // atrace
} // loom
} // facebook
