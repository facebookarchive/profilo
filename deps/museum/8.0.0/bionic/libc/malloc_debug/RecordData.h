/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef DEBUG_MALLOC_RECORDDATA_H
#define DEBUG_MALLOC_RECORDDATA_H

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <string>

#include <private/bionic_macros.h>

class RecordEntry {
 public:
  RecordEntry();
  virtual ~RecordEntry() = default;

  virtual std::string GetString() const = 0;

 protected:
  pid_t tid_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RecordEntry);
};

class ThreadCompleteEntry : public RecordEntry {
 public:
  ThreadCompleteEntry() = default;
  virtual ~ThreadCompleteEntry() = default;

  std::string GetString() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadCompleteEntry);
};

class AllocEntry : public RecordEntry {
 public:
  AllocEntry(void* pointer);
  virtual ~AllocEntry() = default;

 protected:
  void* pointer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllocEntry);
};

class MallocEntry : public AllocEntry {
 public:
  MallocEntry(void* pointer, size_t size);
  virtual ~MallocEntry() = default;

  std::string GetString() const override;

 protected:
  size_t size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MallocEntry);
};

class FreeEntry : public AllocEntry {
 public:
  FreeEntry(void* pointer);
  virtual ~FreeEntry() = default;

  std::string GetString() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FreeEntry);
};

class CallocEntry : public MallocEntry {
 public:
  CallocEntry(void* pointer, size_t size, size_t nmemb);
  virtual ~CallocEntry() = default;

  std::string GetString() const override;

 protected:
  size_t nmemb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallocEntry);
};

class ReallocEntry : public MallocEntry {
 public:
  ReallocEntry(void* pointer, size_t size, void* old_pointer);
  virtual ~ReallocEntry() = default;

  std::string GetString() const override;

 protected:
  void* old_pointer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReallocEntry);
};

// posix_memalign, memalign, pvalloc, valloc all recorded with this class.
class MemalignEntry : public MallocEntry {
 public:
  MemalignEntry(void* pointer, size_t size, size_t alignment);
  virtual ~MemalignEntry() = default;

  std::string GetString() const override;

 protected:
  size_t alignment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemalignEntry);
};

struct Config;

class RecordData {
 public:
  RecordData();
  virtual ~RecordData();

  bool Initialize(const Config& config);

  void AddEntry(const RecordEntry* entry);
  void AddEntryOnly(const RecordEntry* entry);

  void SetToDump() { dump_ = true; }

  pthread_key_t key() { return key_; }

 private:
  void Dump();

  std::mutex dump_lock_;
  pthread_key_t key_;
  const RecordEntry** entries_ = nullptr;
  size_t num_entries_ = 0;
  std::atomic_uint cur_index_;
  std::atomic_bool dump_;
  std::string dump_file_;

  DISALLOW_COPY_AND_ASSIGN(RecordData);
};

#endif // DEBUG_MALLOC_RECORDDATA_H
