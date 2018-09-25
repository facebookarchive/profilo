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

#include <cppdistract/dso.h>
#include <dlfcn.h>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace facebook {
namespace cppdistract {

dso::dso(char const* name) : handle_(dlopen(name, RTLD_LOCAL)) {
  if (!handle_) {
    throw std::runtime_error(dlerror());
  }
}

dso::~dso() {
  dlclose(handle_);
}

void* dso::get_handle() const {
  return handle_;
}

void* dso::get_symbol_internal(
    std::initializer_list<char const*> const& names) const {
  for (auto name : names) {
    void* symbol = dlsym(handle_, name);
    if (symbol) {
      return symbol;
    }
  }
  std::stringstream error;
  error << "Failed to find any of: ";
  std::copy(
      names.begin(),
      names.end(),
      std::ostream_iterator<std::string>(error, ","));
  throw std::runtime_error(error.str());
}

} // namespace cppdistract
} // namespace facebook
