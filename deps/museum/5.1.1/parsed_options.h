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
#include "instruction_set.h"
#include "profiler_options.h"

namespace art {

class CompilerCallbacks;
class DexFile;

typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;

class ParsedOptions {
 public:
  // returns null if problem parsing and ignore_unrecognized is false
  static ParsedOptions* Create(const RuntimeOptions& options, bool ignore_unrecognized);

  const std::vector<const DexFile*>* boot_class_path_;
  std::string boot_class_path_string_;
  std::string class_path_string_;
  std::string image_;
  bool check_jni_;
  std::string jni_trace_;
  std::string native_bridge_library_filename_;
  CompilerCallbacks* compiler_callbacks_;
  bool is_zygote_;
  bool must_relocate_;
  bool dex2oat_enabled_;
  bool image_dex2oat_enabled_;
  std::string patchoat_executable_;
  bool interpreter_only_;
  bool is_explicit_gc_disabled_;
  bool use_tlab_;
  bool verify_pre_gc_heap_;
  bool verify_pre_sweeping_heap_;
  bool verify_post_gc_heap_;
  bool verify_pre_gc_rosalloc_;
  bool verify_pre_sweeping_rosalloc_;
  bool verify_post_gc_rosalloc_;
  unsigned int long_pause_log_threshold_;
  unsigned int long_gc_log_threshold_;
  bool dump_gc_performance_on_shutdown_;
  bool ignore_max_footprint_;
  size_t heap_initial_size_;
  size_t heap_maximum_size_;
  size_t heap_growth_limit_;
  size_t heap_min_free_;
  size_t heap_max_free_;
  size_t heap_non_moving_space_capacity_;
  double heap_target_utilization_;
  double foreground_heap_growth_multiplier_;
  unsigned int parallel_gc_threads_;
  unsigned int conc_gc_threads_;
  gc::CollectorType collector_type_;
  gc::CollectorType background_collector_type_;
  size_t stack_size_;
  unsigned int max_spins_before_thin_lock_inflation_;
  bool low_memory_mode_;
  unsigned int lock_profiling_threshold_;
  std::string stack_trace_file_;
  bool method_trace_;
  std::string method_trace_file_;
  unsigned int method_trace_file_size_;
  bool (*hook_is_sensitive_thread_)();
  jint (*hook_vfprintf_)(FILE* stream, const char* format, va_list ap);
  void (*hook_exit_)(jint status);
  void (*hook_abort_)();
  std::vector<std::string> properties_;
  std::string compiler_executable_;
  std::vector<std::string> compiler_options_;
  std::vector<std::string> image_compiler_options_;
  ProfilerOptions profiler_options_;
  std::string profile_output_filename_;
  TraceClockSource profile_clock_source_;
  bool verify_;
  InstructionSet image_isa_;

  // Whether or not we use homogeneous space compaction to avoid OOM errors. If enabled,
  // the heap will attempt to create an extra space which enables compacting from a malloc space to
  // another malloc space when we are about to throw OOM.
  bool use_homogeneous_space_compaction_for_oom_;
  // Minimal interval allowed between two homogeneous space compactions caused by OOM.
  uint64_t min_interval_homogeneous_space_compaction_by_oom_;

 private:
  ParsedOptions() {}

  void Usage(const char* fmt, ...);
  void UsageMessage(FILE* stream, const char* fmt, ...);
  void UsageMessageV(FILE* stream, const char* fmt, va_list ap);

  void Exit(int status);
  void Abort();

  bool Parse(const RuntimeOptions& options,  bool ignore_unrecognized);
  bool ParseXGcOption(const std::string& option);
  bool ParseStringAfterChar(const std::string& option, char after_char, std::string* parsed_value);
  bool ParseInteger(const std::string& option, char after_char, int* parsed_value);
  bool ParseUnsignedInteger(const std::string& option, char after_char, unsigned int* parsed_value);
  bool ParseDouble(const std::string& option, char after_char, double min, double max,
                   double* parsed_value);
};

}  // namespace art

#endif  // ART_RUNTIME_PARSED_OPTIONS_H_
