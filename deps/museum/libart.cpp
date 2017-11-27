// Copyright 2004-present Facebook. All Rights Reserved.

#include <vector>

#include <cppdistract/dso.h>

using namespace facebook::cppdistract;

namespace facebook {

static std::vector<void*(*)()>& artSymbolLookups() {
  static std::vector<void*(*)()> artSymbolLookups_;
  return artSymbolLookups_;
}

dso const& libart() {
  static dso const libart("libart.so");
  return libart;
}

void registerSymbolLookup(void*(*lookup)()) {
  artSymbolLookups().push_back(lookup);
}

void preinitSymbols() {
  for (auto symbolLookup : artSymbolLookups()) {
    symbolLookup();
  }
}

} // namespace facebook
