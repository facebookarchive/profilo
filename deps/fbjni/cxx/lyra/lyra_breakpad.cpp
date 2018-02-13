// Copyright 2004-present Facebook. All Rights Reserved.

#include <lyra/lyra.h>

namespace facebook {
namespace lyra {

/**
 * This can be overridden by an implementation capable of looking up
 * the breakpad id for logging purposes.
*/
__attribute__((weak))
std::string getBreakpadId(const std::string& library) {
  return "<unimplemented>";
}

}
}
