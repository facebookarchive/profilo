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

#include "test.h"
#ifdef ANDROID
#include <cstdio>
#include <string>
#include <android/log.h>
#endif

std::unique_ptr<LibraryHandle> loadLibrary(const char* name) {
#ifdef ANDROID
  // Read the current app name from cmdline, then use that to construct a path to the library
  FILE* cmdline = fopen("/proc/self/cmdline", "r");
  char procname[255]{};
  fread(procname, sizeof(procname), 1, cmdline);
  fclose(cmdline);

  try {
    auto path = std::string("/data/data/") + std::string(procname) + "/lib/" + name;
    return std::make_unique<LibraryHandle>(path.c_str());
  } catch (...) {
    __android_log_print(ANDROID_LOG_DEBUG, "test.h:loadLibrary", "Fallback loading of %s", name);
    return std::make_unique<LibraryHandle>(name);
  }
#else
  return std::make_unique<LibraryHandle>(name);
#endif
}
