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

#pragma once

#include <linker/linker.h>
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <stdexcept>

namespace facebook { namespace linker {
  void clearSharedLibs(); // testing only
} } // namespace facebook::linker

// RAII dlopen handle
struct LibraryHandle {
  explicit LibraryHandle(const char* name):
    handle(dlopen(name, RTLD_NOW|RTLD_LOCAL)) {
    if (handle == nullptr) {
      std::string error("Could not load ");
      std::string dlerr(dlerror());
      throw std::invalid_argument(error + name + dlerr);
    }
  }

  LibraryHandle(const LibraryHandle&) = delete;
  LibraryHandle& operator=(const LibraryHandle&) = delete;

  LibraryHandle(LibraryHandle&& other) {
    handle = other.handle;
    other.handle = nullptr;
  }

  ~LibraryHandle() {
    if (handle != nullptr) {
      dlclose(handle);
    }
  }

  template <typename T>
  T* get_symbol(char const* name) const {
    auto result = (T*) dlsym(handle, name);
    if (result == nullptr) {
      throw std::invalid_argument(std::string("Could not find symbol: ") + name);
    }
    return result;
  }

private:
  void* handle = nullptr;
};

struct BaseTest : public testing::Test {
  virtual void SetUp() {
    linker_set_enabled(true);
    ASSERT_EQ(0, linker_initialize());
  }

  ~BaseTest() {
    facebook::linker::clearSharedLibs();
  }
};

std::unique_ptr<LibraryHandle> loadLibrary(const char* name);
