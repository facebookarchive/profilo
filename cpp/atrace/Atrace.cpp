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

#include <dlfcn.h>
#include <libgen.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>

#include <fb/Build.h>
#include <fb/log.h>
#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <linker/sharedlibs.h>
#include <plthooks/plthooks.h>
#include <profilo/util/common.h>
#include <profilo/util/hooks.h>

#include <profilo/LogEntry.h>
#include <profilo/TraceProviders.h>
#include <profilo/jni/JMultiBufferLogger.h>
#include <profilo/logger/BlockLogger.h>

#include "Atrace.h"

namespace fbjni = facebook::jni;

namespace facebook {
namespace profilo {
namespace atrace {

namespace {

int* atrace_marker_fd = nullptr;
std::atomic<uint64_t>* atrace_enabled_tags = nullptr;
std::atomic<uint64_t> original_tags(UINT64_MAX);
std::atomic<bool> systrace_installed;
std::atomic<uint32_t> provider_mask;
logger::MultiBufferLogger* logger;
bool first_enable = true;
bool atrace_enabled;

namespace {

ssize_t const kAtraceMessageLength = 1024;
// Magic FD to simply write to tracer logger and bypassing real write
int const kTracerMagicFd = -100;
// Libraries which log to ATRACE reference this function symbol. This symbol is
// used for early verification if a library should be hooked.
constexpr char kAtraceSymbol[] = "atrace_setup";
// Prefix for system libraries.
constexpr char kSysLibPrefix[] = "/system";
// Starting from SDK 27 it's enough to hook single "libcutils" lib to capture
// all ATRACE logging.
constexpr auto kSingleLibMinSdk = 27;
constexpr auto kLibWhitelistMinSdk = 23;
constexpr char kSingleLibName[] = "libcutils.so";

// Determine if this library should be hooked.
bool allowHookingCb(char const* libname, char const* full_libname, void* data) {
  std::unordered_set<std::string>* seenLibs =
      static_cast<std::unordered_set<std::string>*>(data);

  if (seenLibs->find(libname) != seenLibs->cend()) {
    // We already hooked (or saw and decided not to hook) this library.
    return false;
  }

  seenLibs->insert(libname);

  // Only allow to hook system libraries
  if (strncmp(full_libname, kSysLibPrefix, sizeof(kSysLibPrefix) - 1)) {
    return false;
  }

  // Verify if the library contains atrace indicator symbol, otherwise we
  // don't need to install hooks.
  auto result = linker::sharedLib(libname);
  if (!result.success) {
    return false;
  }
  ElfW(Sym) const* sym = result.data.find_symbol_by_name(kAtraceSymbol);
  if (!sym) {
    return false;
  }

  return true;
}

void log_systrace(const void* buf, size_t count) {
  const char* msg = reinterpret_cast<const char*>(buf);

  EntryType type;
  switch (msg[0]) {
    case 'B': { // begin synchronous event. format: "B|<pid>|<name>"
      type = EntryType::MARK_PUSH;
      break;
    }
    case 'E': { // end synchronous event. format: "E"
      type = EntryType::MARK_POP;
      break;
    }
    // the following events we don't currently log.
    case 'S': // start async event. format: "S|<pid>|<name>|<cookie>"
    case 'F': // finish async event. format: "F|<pid>|<name>|<cookie>"
    case 'C': // counter. format: "C|<pid>|<name>|<value>"
    default:
      return;
  }

  StandardEntry entry{};
  entry.tid = threadID();
  entry.timestamp = monotonicTime();
  entry.type = type;

  int32_t id = logger->write(entry);
  if (type != EntryType::MARK_POP) {
    // Format is B|<pid>|<name>.
    // Skip "B|" trivially, find next '|' with memchr. We cannot use strchr
    // since we can't trust the message to have a null terminator.
    constexpr auto kPrefixLength = 2; // length of "B|";

    const char* name = reinterpret_cast<const char*>(
        memchr(msg + kPrefixLength, '|', count - kPrefixLength));
    if (name == nullptr) {
      return;
    }
    name++; // skip '|' to the next character
    ssize_t len = msg + count - name;
    if (len > 0) {
      logger->writeBytes(
          EntryType::STRING_NAME,
          id,
          (const uint8_t*)name,
          std::min(len, kAtraceMessageLength));

      FBLOGV("systrace event: %s", name);
    }
  }
}

bool should_log_systrace(int fd, size_t count) {
  return (
      systrace_installed && fd == *atrace_marker_fd &&
      TraceProviders::get().isEnabled(provider_mask.load()) && count > 0);
}

ssize_t write_hook(int fd, const void* buf, size_t count) {
  if (should_log_systrace(fd, count)) {
    log_systrace(buf, count);
    return count;
  }
  return CALL_PREV(write_hook, fd, buf, count);
}

ssize_t
__write_chk_hook(int fd, const void* buf, size_t count, size_t buf_size) {
  if (should_log_systrace(fd, count)) {
    log_systrace(buf, count);
    return count;
  }
  return CALL_PREV(__write_chk_hook, fd, buf, count, buf_size);
}

constexpr auto kWhitelistSize = 7;

std::array<std::string, kWhitelistSize>& getLibWhitelist() {
  static std::array<std::string, kWhitelistSize> whitelist = {
      {"libandroid_runtime.so",
       "libui.so",
       "libgui.so",
       "libart.so",
       "libhwui.so",
       "libEGL.so",
       "libcutils.so"}};
  return whitelist;
}

} // namespace

std::vector<plt_hook_spec>& getFunctionHooks() {
  static std::vector<plt_hook_spec> functionHooks = {
      {"libc.so", "write", reinterpret_cast<void*>(&write_hook)},
      {"libc.so", "__write_chk", reinterpret_cast<void*>(__write_chk_hook)},
  };
  return functionHooks;
}

plt_hook_spec& getSingleLibFunctionSpec() {
  static plt_hook_spec spec{
      "__write_chk", reinterpret_cast<void*>(__write_chk_hook)};
  return spec;
}

// Returns the set of libraries that we don't want to hook.
std::unordered_set<std::string>& getSeenLibs() {
  static std::unordered_set<std::string> seenLibs;

  // Add this library's name to the set that we won't hook
  if (seenLibs.size() == 0) {
    seenLibs.insert("libc.so");

    Dl_info info;
    if (!dladdr((void*)&getSeenLibs, &info)) {
      FBLOGV("Failed to find module name");
    }
    if (info.dli_fname == nullptr) {
      // Not safe to continue as a thread may block trying to hook the current
      // library
      throw std::runtime_error("could not resolve current library");
    }

    seenLibs.insert(basename(info.dli_fname));
  }
  return seenLibs;
}

void hookLoadedLibs() {
  logger::BlockLogger block(*logger, "hookLoadedLibs");

  auto sdk = build::Build::getAndroidSdk();
  if (sdk >= kSingleLibMinSdk) {
    auto& spec = getSingleLibFunctionSpec();
    hook_single_lib(kSingleLibName, &spec, 1);
    return;
  }

  if (sdk >= kLibWhitelistMinSdk) {
    auto& whitelist = getLibWhitelist();
    auto& functionSpecs = getFunctionHooks();
    for (auto& lib : whitelist) {
      auto failures = hook_single_lib(
          lib.c_str(), functionSpecs.data(), functionSpecs.size());
      if (failures) {
        throw std::runtime_error("Hook failed for library: " + lib);
      }
    }
    return;
  }

  auto& functionHooks = getFunctionHooks();
  auto& seenLibs = getSeenLibs();

  facebook::profilo::hooks::hookLoadedLibs(
      functionHooks, allowHookingCb, &seenLibs);
}

void unhookLoadedLibs() {
  auto sdk = build::Build::getAndroidSdk();
  if (sdk >= kSingleLibMinSdk) {
    auto& spec = getSingleLibFunctionSpec();
    unhook_single_lib(kSingleLibName, &spec, 1);
    return;
  }

  if (sdk >= kLibWhitelistMinSdk) {
    auto& whitelist = getLibWhitelist();
    auto& functionSpecs = getFunctionHooks();
    for (auto& lib : whitelist) {
      auto failures = unhook_single_lib(
          lib.c_str(), functionSpecs.data(), functionSpecs.size());
      if (failures) {
        throw std::runtime_error("Unhook failed for library: " + lib);
      }
    }
    return;
  }

  auto& functionHooks = getFunctionHooks();

  facebook::profilo::hooks::unhookLoadedLibs(functionHooks);
  getSeenLibs().clear();
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

    atrace_enabled_tags = reinterpret_cast<std::atomic<uint64_t>*>(
        dlsym(handle, enabled_tags_sym.c_str()));

    if (atrace_enabled_tags == nullptr) {
      throw std::runtime_error("Enabled Tags not defined");
    }

    atrace_marker_fd = reinterpret_cast<int*>(dlsym(handle, fd_sym.c_str()));

    if (atrace_marker_fd == nullptr) {
      throw std::runtime_error("Trace FD not defined");
    }
    if (*atrace_marker_fd == -1) {
      // This is a case that can happen for older Android version i.e. 4.4
      // in which scenario the marker fd is not initialized/opened  by Zygote.
      // Nevertheless for Profilo trace it is not necessary to have an open fd,
      // since all we really need is to ensure that we 'know' it is marker
      // fd to continue writing Profilo logs, thus the usage of marker fd
      // acting really as a placeholder for magic id.
      *atrace_marker_fd = kTracerMagicFd;
    }
  }

  if (plthooks_initialize()) {
    throw std::runtime_error("Could not initialize plthooks library");
  }

  hookLoadedLibs();

  systrace_installed = true;
  provider_mask = providerMask;
}

void enableSystrace() {
  logger::BlockLogger block(*logger, "enableSystrace");

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
  if (prev != UINT64_MAX) { // if we somehow call this twice in a row, don't
                            // overwrite the real tags
    original_tags = prev;
  }

  atrace_enabled = true;
}

void restoreSystrace() {
  logger::BlockLogger block(*logger, "restoreSystrace");

  atrace_enabled = false;
  if (!systrace_installed) {
    return;
  }

  try {
    unhookLoadedLibs();
  } catch (...) {
  }

  uint64_t tags = original_tags;
  if (tags != UINT64_MAX) { // if we somehow call this before enableSystrace,
                            // don't screw it up
    atrace_enabled_tags->store(tags);
  }
}

bool installSystraceHook(logger::JMultiBufferLogger* jlogger, int mask) {
  logger = &jlogger->nativeInstance();
  logger::BlockLogger block(*logger, "installSystraceHook");

  try {
    installSystraceSnooper(mask);
    return true;
  } catch (const std::runtime_error& e) {
    FBLOGW("could not install hooks: %s", e.what());
    return false;
  }
}

} // namespace

bool JNI_installSystraceHook(
    fbjni::alias_ref<jobject>,
    logger::JMultiBufferLogger* logger,
    jint mask) {
  return installSystraceHook(logger, mask);
}

void JNI_enableSystraceNative(fbjni::alias_ref<jobject>) {
  enableSystrace();
}

void JNI_restoreSystraceNative(fbjni::alias_ref<jobject>) {
  restoreSystrace();
}

bool JNI_isEnabled(fbjni::alias_ref<jobject>) {
  return atrace_enabled;
}

namespace fbjni = facebook::jni;

void registerNatives() {
  fbjni::registerNatives(
      "com/facebook/profilo/provider/atrace/Atrace",
      {
          makeNativeMethod("installSystraceHook", JNI_installSystraceHook),
          makeNativeMethod("enableSystraceNative", JNI_enableSystraceNative),
          makeNativeMethod("restoreSystraceNative", JNI_restoreSystraceNative),
          makeNativeMethod("isEnabled", JNI_isEnabled),
      });
}

} // namespace atrace
} // namespace profilo
} // namespace facebook
