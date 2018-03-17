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

#ifndef DEBUG_MALLOC_FREETRACKDATA_H
#define DEBUG_MALLOC_FREETRACKDATA_H

#include <stdint.h>
#include <pthread.h>

#include <deque>
#include <unordered_map>
#include <vector>

#include <private/bionic_macros.h>

// Forward declarations.
struct Header;
class DebugData;
struct Config;
struct BacktraceHeader;

class FreeTrackData {
 public:
  FreeTrackData(const Config& config);
  virtual ~FreeTrackData() = default;

  void Add(DebugData& debug, const Header* header);

  void VerifyAll(DebugData& debug);

  void LogBacktrace(const Header* header);

 private:
  void LogFreeError(DebugData& debug, const Header* header, const uint8_t* pointer);
  void VerifyAndFree(DebugData& debug, const Header* header, const void* pointer);

  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  std::deque<const Header*> list_;
  std::vector<uint8_t> cmp_mem_;
  std::unordered_map<const Header*, BacktraceHeader*> backtraces_;
  size_t backtrace_num_frames_;

  DISALLOW_COPY_AND_ASSIGN(FreeTrackData);
};

#endif // DEBUG_MALLOC_FREETRACKDATA_H
