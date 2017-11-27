// Copyright 2004-present Facebook. All Rights Reserved.

#include <cppdistract/dso.h>
#include <dlfcn.h>
#include <stdexcept>
#include <initializer_list>
#include <iterator>
#include <sstream>

namespace facebook { namespace cppdistract {

dso::dso(char const* name)
  : handle_(dlopen(name, RTLD_LOCAL)){
  if (!handle_) {
    throw std::runtime_error("Failed to open " + std::string(name));
  }
}

dso::~dso() {
  if (dlclose(handle_) == -1) {
    throw std::runtime_error(dlerror());
  }
}

void* dso::get_handle() const {
  return handle_;
}

void* dso::get_symbol_internal(std::initializer_list<char const*> const& names) const {
  for (auto name : names) {
    void *symbol = dlsym(handle_, name);
    if (symbol) {
      return symbol;
    }
  }
  std::stringstream error;
  error << "Failed to find any of: ";
  std::copy(names.begin(), names.end(), std::ostream_iterator<std::string>(error, ","));
  throw std::runtime_error(error.str());
}

} } // namespace facebook::cppdistract
