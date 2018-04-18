// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <string>

namespace facebook {
namespace profilo {
namespace mappingdensity {

void dumpMappingDensities(
  std::string const& mapRegexStr,
  std::string const& outFile,
  std::string const& dumpName);

} } } // namespace facebook::profilo::mappingdensity
