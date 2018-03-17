/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_TO_STR_H_
#define ART_RUNTIME_BASE_TO_STR_H_

#include <sstream>

namespace art {

// Helps you use operator<< in a const char*-like context such as our various 'F' methods with
// format strings.
template<typename T>
class ToStr {
 public:
  explicit ToStr(const T& value) {
    std::ostringstream os;
    os << value;
    s_ = os.str();
  }

  const char* c_str() const {
    return s_.c_str();
  }

  const std::string& str() const {
    return s_;
  }

 private:
  std::string s_;
  DISALLOW_COPY_AND_ASSIGN(ToStr);
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_TO_STR_H_
