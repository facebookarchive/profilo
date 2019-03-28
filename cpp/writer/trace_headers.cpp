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

#include "trace_headers.h"

#include <errno.h>
#include <sys/system_properties.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace facebook {
namespace profilo {
namespace writer {

std::vector<std::pair<std::string, std::string>> calculateHeaders() {
  auto result = std::vector<std::pair<std::string, std::string>>();
  result.reserve(4);

  {
    std::stringstream ss;
    ss << getpid();
    result.push_back(std::make_pair("pid", ss.str()));
  }

  {
    struct utsname name {};
    if (uname(&name)) {
      throw std::system_error(
          errno, std::system_category(), "could not uname(2)");
    }

    result.push_back(std::make_pair("arch", std::string(name.machine)));
  }

  {
    char prop_value[PROP_VALUE_MAX]{};
    if (__system_property_get("ro.build.version.release", prop_value) > 0) {
      std::stringstream ss;
      ss << "Android" << prop_value;

      result.push_back(std::make_pair("os", ss.str()));
    }
  }

  return result;
}

} // namespace writer
} // namespace profilo
} // namespace facebook
