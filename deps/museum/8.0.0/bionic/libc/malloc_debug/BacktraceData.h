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

#ifndef DEBUG_MALLOC_BACKTRACEDATA_H
#define DEBUG_MALLOC_BACKTRACEDATA_H

#include <stdint.h>

#include <private/bionic_macros.h>

#include "OptionData.h"

// Forward declarations.
struct Config;

class BacktraceData : public OptionData {
 public:
  BacktraceData(DebugData* debug_data, const Config& config, size_t* offset);
  virtual ~BacktraceData() = default;

  bool Initialize(const Config& config);

  inline size_t alloc_offset() { return alloc_offset_; }

  bool enabled() { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  size_t alloc_offset_ = 0;

  volatile bool enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(BacktraceData);
};

#endif // DEBUG_MALLOC_BACKTRACEDATA_H
