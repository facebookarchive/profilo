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

struct Config {
  bool Set(const char* str);

  size_t front_guard_bytes = 0;
  size_t rear_guard_bytes = 0;

  bool backtrace_enable_on_signal = false;
  int backtrace_signal = 0;
  bool backtrace_enabled = false;
  size_t backtrace_frames = 0;

  size_t fill_on_alloc_bytes = 0;
  size_t fill_on_free_bytes = 0;

  size_t expand_alloc_bytes = 0;

  size_t free_track_allocations = 0;
  size_t free_track_backtrace_num_frames = 0;

  int record_allocs_signal = 0;
  size_t record_allocs_num_entries = 0;
  std::string record_allocs_file;

  uint64_t options = 0;
  uint8_t fill_alloc_value;
  uint8_t fill_free_value;
  uint8_t front_guard_value;
  uint8_t rear_guard_value;
};

#endif  // MALLOC_DEBUG_CONFIG_H
