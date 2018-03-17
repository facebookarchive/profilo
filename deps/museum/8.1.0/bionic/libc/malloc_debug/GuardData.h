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

#ifndef DEBUG_MALLOC_GUARDDATA_H
#define DEBUG_MALLOC_GUARDDATA_H

#include <stdint.h>
#include <string.h>

#include <vector>

#include <private/bionic_macros.h>

#include "OptionData.h"

// Forward declarations.
class DebugData;
struct Header;
class Config;

class GuardData : public OptionData {
 public:
  GuardData(DebugData* debug_data, int init_value, size_t num_bytes);
  virtual ~GuardData() = default;

  bool Valid(void* data) { return memcmp(data, cmp_mem_.data(), cmp_mem_.size()) == 0; }

  void LogFailure(const Header* header, const void* pointer, const void* data);

 protected:
  std::vector<uint8_t> cmp_mem_;

  virtual const char* GetTypeName() = 0;

  DISALLOW_COPY_AND_ASSIGN(GuardData);
};

class FrontGuardData : public GuardData {
 public:
  FrontGuardData(DebugData* debug_data, const Config& config, size_t* offset);
  virtual ~FrontGuardData() = default;

  bool Valid(const Header* header);

  void LogFailure(const Header* header);

  size_t offset() { return offset_; }

 private:
  const char* GetTypeName() override { return "FRONT"; }

  size_t offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FrontGuardData);
};

class RearGuardData : public GuardData {
 public:
  RearGuardData(DebugData* debug_data, const Config& config);
  virtual ~RearGuardData() = default;

  bool Valid(const Header* header);

  void LogFailure(const Header* header);

 private:
  const char* GetTypeName() override { return "REAR"; }

  DISALLOW_COPY_AND_ASSIGN(RearGuardData);
};

#endif // DEBUG_MALLOC_GUARDDATA_H
