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

#ifndef ART_RUNTIME_PARSED_OPTIONS_H_
#define ART_RUNTIME_PARSED_OPTIONS_H_

#include <string>
#include <vector>

#include <jni.h>

#include "globals.h"
#include "gc/collector_type.h"
#include "gc/space/large_object_space.h"
#include "arch/instruction_set.h"
#include "profiler_options.h"
#include "runtime_options.h"

namespace art {

class CompilerCallbacks;
class DexFile;
struct RuntimeArgumentMap;

typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;

template <typename TVariantMap,
          template <typename TKeyValue> class TVariantMapKey>
struct CmdlineParser;

class ParsedOptions {
 public:
  using RuntimeParser = CmdlineParser<RuntimeArgumentMap, RuntimeArgumentMap::Key>;
  // Create a parser that can turn user-defined input into a RuntimeArgumentMap.
  // This visibility is effectively for testing-only, and normal code does not
  // need to create its own parser.
  static std::unique_ptr<RuntimeParser> MakeParser(bool ignore_unrecognized);

  // returns true if parsing succeeds, and stores the resulting options into runtime_options
  static bool Parse(const RuntimeOptions& options,
                    bool ignore_unrecognized,
                    RuntimeArgumentMap* runtime_options);

  bool (*hook_is_sensitive_thread_)();
  jint (*hook_vfprintf_)(FILE* stream, const char* format, va_list ap);
  void (*hook_exit_)(jint status);
  void (*hook_abort_)();

 private:
  ParsedOptions();

  bool ProcessSpecialOptions(const RuntimeOptions& options,
                             RuntimeArgumentMap* runtime_options,
                             std::vector<std::string>* out_options);

  void Usage(const char* fmt, ...);
  void UsageMessage(FILE* stream, const char* fmt, ...);
  void UsageMessageV(FILE* stream, const char* fmt, va_list ap);

  void Exit(int status);
  void Abort();

  bool DoParse(const RuntimeOptions& options,
               bool ignore_unrecognized,
               RuntimeArgumentMap* runtime_options);
};

}  // namespace art

#endif  // ART_RUNTIME_PARSED_OPTIONS_H_
