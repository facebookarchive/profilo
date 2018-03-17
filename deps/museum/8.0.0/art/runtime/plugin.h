/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_PLUGIN_H_
#define ART_RUNTIME_PLUGIN_H_

#include <string>
#include "base/logging.h"

namespace art {

// This function is loaded from the plugin (if present) and called during runtime initialization.
// By the time this has been called the runtime has been fully initialized but not other native
// libraries have been loaded yet. Failure to initialize is considered a fatal error.
// TODO might want to give initialization function some arguments
using PluginInitializationFunction = bool (*)();
using PluginDeinitializationFunction = bool (*)();

// A class encapsulating a plugin. There is no stable plugin ABI or API and likely never will be.
// TODO Might want to put some locking in this but ATM we only load these at initialization in a
// single-threaded fashion so not much need
class Plugin {
 public:
  static Plugin Create(const std::string& lib) {
    return Plugin(lib);
  }

  bool IsLoaded() const {
    return dlopen_handle_ != nullptr;
  }

  const std::string& GetLibrary() const {
    return library_;
  }

  bool Load(/*out*/std::string* error_msg);
  bool Unload();


  ~Plugin() {
    if (IsLoaded() && !Unload()) {
      LOG(ERROR) << "Error unloading " << this;
    }
  }

  Plugin(const Plugin& other);

  // Create move constructor for putting this in a list
  Plugin(Plugin&& other)
      : library_(other.library_),
        dlopen_handle_(other.dlopen_handle_) {
    other.dlopen_handle_ = nullptr;
  }

 private:
  explicit Plugin(const std::string& library) : library_(library), dlopen_handle_(nullptr) { }

  std::string library_;
  void* dlopen_handle_;

  friend std::ostream& operator<<(std::ostream &os, Plugin const& m);
};

std::ostream& operator<<(std::ostream &os, Plugin const& m);
std::ostream& operator<<(std::ostream &os, const Plugin* m);

}  // namespace art

#endif  // ART_RUNTIME_PLUGIN_H_
