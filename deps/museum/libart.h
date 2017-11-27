// Copyright 2004-present Facebook. All Rights Reserved.

#include <cppdistract/dso.h>

namespace facebook {

/**
 * Returns a dso object for libart.so
 */
cppdistract::dso const& libart();

void preinitSymbols();

} // namespace facebook
