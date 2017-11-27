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

#ifndef ART_RUNTIME_BASE_UNIX_FILE_NULL_FILE_H_
#define ART_RUNTIME_BASE_UNIX_FILE_NULL_FILE_H_

#include "base/unix_file/random_access_file.h"
#include "base/macros.h"

namespace unix_file {

// A RandomAccessFile implementation equivalent to /dev/null. Writes are
// discarded, and there's no data to be read. Callers could use FdFile in
// conjunction with /dev/null, but that's not portable and costs a file
// descriptor. NullFile is "free".
//
// Thread safe.
class NullFile : public RandomAccessFile {
 public:
  NullFile();
  virtual ~NullFile();

  // RandomAccessFile API.
  virtual int Close();
  virtual int Flush();
  virtual int64_t Read(char* buf, int64_t byte_count, int64_t offset) const;
  virtual int SetLength(int64_t new_length);
  virtual int64_t GetLength() const;
  virtual int64_t Write(const char* buf, int64_t byte_count, int64_t offset);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullFile);
};

}  // namespace unix_file

#endif  // ART_RUNTIME_BASE_UNIX_FILE_NULL_FILE_H_
