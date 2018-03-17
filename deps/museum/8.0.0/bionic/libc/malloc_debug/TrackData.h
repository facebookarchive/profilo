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

#ifndef DEBUG_MALLOC_TRACKDATA_H
#define DEBUG_MALLOC_TRACKDATA_H

#include <stdint.h>
#include <pthread.h>

#include <vector>
#include <unordered_set>

#include <private/bionic_macros.h>

#include "OptionData.h"

// Forward declarations.
struct Header;
struct Config;
class DebugData;

class TrackData : public OptionData {
 public:
  TrackData(DebugData* debug_data);
  virtual ~TrackData() = default;

  void GetList(std::vector<const Header*>* list);

  void Add(const Header* header, bool backtrace_found);

  void Remove(const Header* header, bool backtrace_found);

  bool Contains(const Header *header);

  void GetInfo(uint8_t** info, size_t* overall_size, size_t* info_size,
               size_t* total_memory, size_t* backtrace_size);

  void DisplayLeaks();

  void PrepareFork() { pthread_mutex_lock(&mutex_); }
  void PostForkParent() { pthread_mutex_unlock(&mutex_); }
  void PostForkChild() { pthread_mutex_init(&mutex_, NULL); }

 private:
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  std::unordered_set<const Header*> headers_;
  size_t total_backtrace_allocs_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TrackData);
};

#endif // DEBUG_MALLOC_TRACKDATA_H
