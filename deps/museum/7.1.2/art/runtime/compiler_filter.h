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

#ifndef ART_RUNTIME_COMPILER_FILTER_H_
#define ART_RUNTIME_COMPILER_FILTER_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/macros.h"

namespace art {

class CompilerFilter FINAL {
 public:
  // Note: Order here matters. Later filter choices are considered "as good
  // as" earlier filter choices.
  enum Filter {
    kVerifyNone,          // Skip verification but mark all classes as verified anyway.
    kVerifyAtRuntime,     // Delay verication to runtime, do not compile anything.
    kVerifyProfile,       // Verify only the classes in the profile, compile only JNI stubs.
    kInterpretOnly,       // Verify everything, compile only JNI stubs.
    kTime,                // Compile methods, but minimize compilation time.
    kSpaceProfile,        // Maximize space savings based on profile.
    kSpace,               // Maximize space savings.
    kBalanced,            // Good performance return on compilation investment.
    kSpeedProfile,        // Maximize runtime performance based on profile.
    kSpeed,               // Maximize runtime performance.
    kEverythingProfile,   // Compile everything capable of being compiled based on profile.
    kEverything,          // Compile everything capable of being compiled.
  };

  // Returns true if an oat file with this compiler filter contains
  // compiled executable code for bytecode.
  static bool IsBytecodeCompilationEnabled(Filter filter);

  // Returns true if an oat file with this compiler filter contains
  // compiled executable code for JNI methods.
  static bool IsJniCompilationEnabled(Filter filter);

  // Returns true if this compiler filter requires running verification.
  static bool IsVerificationEnabled(Filter filter);

  // Returns true if an oat file with this compiler filter depends on the
  // boot image checksum.
  static bool DependsOnImageChecksum(Filter filter);

  // Returns true if an oat file with this compiler filter depends on a
  // profile.
  static bool DependsOnProfile(Filter filter);

  // Returns a non-profile-guided version of the given filter.
  static Filter GetNonProfileDependentFilterFrom(Filter filter);

  // Returns true if the 'current' compiler filter is considered at least as
  // good as the 'target' compilation type.
  // For example: kSpeed is as good as kInterpretOnly, but kInterpretOnly is
  // not as good as kSpeed.
  static bool IsAsGoodAs(Filter current, Filter target);

  // Return the flag name of the given filter.
  // For example: given kVerifyAtRuntime, returns "verify-at-runtime".
  // The name returned corresponds to the name accepted by
  // ParseCompilerFilter.
  static std::string NameOfFilter(Filter filter);

  // Parse the compiler filter from the given name.
  // Returns true and sets filter to the parsed value if name refers to a
  // valid filter. Returns false if no filter matches that name.
  // 'filter' must be non-null.
  static bool ParseCompilerFilter(const char* name, /*out*/Filter* filter);

 private:
  DISALLOW_COPY_AND_ASSIGN(CompilerFilter);
};

std::ostream& operator<<(std::ostream& os, const CompilerFilter::Filter& rhs);

}  // namespace art

#endif  // ART_RUNTIME_COMPILER_FILTER_H_
