/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MALLOC_DEBUG_CONFIG_H
#define MALLOC_DEBUG_CONFIG_H

#include <stdint.h>

#include <string>
#include <unordered_map>

constexpr uint64_t FRONT_GUARD = 0x1;
constexpr uint64_t REAR_GUARD = 0x2;
constexpr uint64_t BACKTRACE = 0x4;
constexpr uint64_t FILL_ON_ALLOC = 0x8;
constexpr uint64_t FILL_ON_FREE = 0x10;
constexpr uint64_t EXPAND_ALLOC = 0x20;
constexpr uint64_t FREE_TRACK = 0x40;
constexpr uint64_t TRACK_ALLOCS = 0x80;
constexpr uint64_t LEAK_TRACK = 0x100;
constexpr uint64_t RECORD_ALLOCS = 0x200;

// In order to guarantee posix compliance, set the minimum alignment
// to 8 bytes for 32 bit systems and 16 bytes for 64 bit systems.
#if defined(__LP64__)
constexpr size_t MINIMUM_ALIGNMENT_BYTES = 16;
#else
constexpr size_t MINIMUM_ALIGNMENT_BYTES = 8;
#endif

// If one or more of these options is set, then a special header is needed.
constexpr uint64_t HEADER_OPTIONS = FRONT_GUARD | REAR_GUARD | BACKTRACE | FREE_TRACK | LEAK_TRACK;

class Config {
 public:
  bool Init(const char* options_str);

  void LogUsage() const;

  uint64_t options() const { return options_; }

  int backtrace_signal() const { return backtrace_signal_; }
  size_t backtrace_frames() const { return backtrace_frames_; }
  size_t backtrace_enabled() const { return backtrace_enabled_; }
  size_t backtrace_enable_on_signal() const { return backtrace_enable_on_signal_; }

  size_t front_guard_bytes() const { return front_guard_bytes_; }
  size_t rear_guard_bytes() const { return rear_guard_bytes_; }
  uint8_t front_guard_value() const { return front_guard_value_; }
  uint8_t rear_guard_value() const { return rear_guard_value_; }

  size_t expand_alloc_bytes() const { return expand_alloc_bytes_; }

  size_t free_track_allocations() const { return free_track_allocations_; }
  size_t free_track_backtrace_num_frames() const { return free_track_backtrace_num_frames_; }

  size_t fill_on_alloc_bytes() const { return fill_on_alloc_bytes_; }
  size_t fill_on_free_bytes() const { return fill_on_free_bytes_; }
  uint8_t fill_alloc_value() const { return fill_alloc_value_; }
  uint8_t fill_free_value() const { return fill_free_value_; }

  int record_allocs_signal() const { return record_allocs_signal_; }
  size_t record_allocs_num_entries() const { return record_allocs_num_entries_; }
  const std::string& record_allocs_file() const { return record_allocs_file_; }

 private:
  struct OptionInfo {
    uint64_t option;
    bool (Config::*process_func)(const std::string&, const std::string&);
  };

  bool ParseValue(const std::string& option, const std::string& value, size_t default_value,
                  size_t min_value, size_t max_value, size_t* new_value) const;

  bool ParseValue(const std::string& option, const std::string& value, size_t min_value,
                  size_t max_value, size_t* parsed_value) const;

  bool SetGuard(const std::string& option, const std::string& value);
  bool SetFrontGuard(const std::string& option, const std::string& value);
  bool SetRearGuard(const std::string& option, const std::string& value);

  bool SetFill(const std::string& option, const std::string& value);
  bool SetFillOnAlloc(const std::string& option, const std::string& value);
  bool SetFillOnFree(const std::string& option, const std::string& value);

  bool SetBacktrace(const std::string& option, const std::string& value);
  bool SetBacktraceEnableOnSignal(const std::string& option, const std::string& value);

  bool SetExpandAlloc(const std::string& option, const std::string& value);

  bool SetFreeTrack(const std::string& option, const std::string& value);
  bool SetFreeTrackBacktraceNumFrames(const std::string& option, const std::string& value);

  bool SetRecordAllocs(const std::string& option, const std::string& value);
  bool SetRecordAllocsFile(const std::string& option, const std::string& value);

  bool VerifyValueEmpty(const std::string& option, const std::string& value);

  static bool GetOption(const char** option_str, std::string* option, std::string* value);

  const static std::unordered_map<std::string, OptionInfo> kOptions;

  size_t front_guard_bytes_ = 0;
  size_t rear_guard_bytes_ = 0;

  bool backtrace_enable_on_signal_ = false;
  int backtrace_signal_ = 0;
  bool backtrace_enabled_ = false;
  size_t backtrace_frames_ = 0;

  size_t fill_on_alloc_bytes_ = 0;
  size_t fill_on_free_bytes_ = 0;

  size_t expand_alloc_bytes_ = 0;

  size_t free_track_allocations_ = 0;
  size_t free_track_backtrace_num_frames_ = 0;

  int record_allocs_signal_ = 0;
  size_t record_allocs_num_entries_ = 0;
  std::string record_allocs_file_;

  uint64_t options_ = 0;
  uint8_t fill_alloc_value_;
  uint8_t fill_free_value_;
  uint8_t front_guard_value_;
  uint8_t rear_guard_value_;
};

#endif  // MALLOC_DEBUG_CONFIG_H
