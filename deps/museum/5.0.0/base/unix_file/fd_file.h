/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_UNIX_FILE_FD_FILE_H_
#define ART_RUNTIME_BASE_UNIX_FILE_FD_FILE_H_

#include <fcntl.h>
#include <string>
#include "base/unix_file/random_access_file.h"
#include "base/macros.h"

namespace unix_file {

// A RandomAccessFile implementation backed by a file descriptor.
//
// Not thread safe.
class FdFile : public RandomAccessFile {
 public:
  FdFile();
  // Creates an FdFile using the given file descriptor. Takes ownership of the
  // file descriptor. (Use DisableAutoClose to retain ownership.)
  explicit FdFile(int fd);
  explicit FdFile(int fd, const std::string& path);

  // Destroys an FdFile, closing the file descriptor if Close hasn't already
  // been called. (If you care about the return value of Close, call it
  // yourself; this is meant to handle failure cases and read-only accesses.
  // Note though that calling Close and checking its return value is still no
  // guarantee that data actually made it to stable storage.)
  virtual ~FdFile();

  // Opens file 'file_path' using 'flags' and 'mode'.
  bool Open(const std::string& file_path, int flags);
  bool Open(const std::string& file_path, int flags, mode_t mode);

  // RandomAccessFile API.
  virtual int Close();
  virtual int64_t Read(char* buf, int64_t byte_count, int64_t offset) const;
  virtual int SetLength(int64_t new_length);
  virtual int64_t GetLength() const;
  virtual int64_t Write(const char* buf, int64_t byte_count, int64_t offset);
  virtual int Flush();

  // Bonus API.
  int Fd() const;
  bool IsOpened() const;
  const std::string& GetPath() const {
    return file_path_;
  }
  void DisableAutoClose();
  bool ReadFully(void* buffer, size_t byte_count);
  bool WriteFully(const void* buffer, size_t byte_count);

 private:
  int fd_;
  std::string file_path_;
  bool auto_close_;

  DISALLOW_COPY_AND_ASSIGN(FdFile);
};

}  // namespace unix_file

#endif  // ART_RUNTIME_BASE_UNIX_FILE_FD_FILE_H_
