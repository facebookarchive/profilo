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

#ifndef ART_RUNTIME_BASE_UNIX_FILE_STRING_FILE_H_
#define ART_RUNTIME_BASE_UNIX_FILE_STRING_FILE_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/stringpiece.h"
#include "base/unix_file/random_access_file.h"

namespace unix_file {

// A RandomAccessFile implementation backed by a std::string. (That is, all data is
// kept in memory.)
//
// Not thread safe.
class StringFile : public RandomAccessFile {
 public:
  StringFile();
  virtual ~StringFile();

  // RandomAccessFile API.
  virtual int Close();
  virtual int Flush();
  virtual int64_t Read(char* buf, int64_t byte_count, int64_t offset) const;
  virtual int SetLength(int64_t new_length);
  virtual int64_t GetLength() const;
  virtual int64_t Write(const char* buf, int64_t byte_count, int64_t offset);

  // Bonus API.
  void Assign(const art::StringPiece& new_data);
  const art::StringPiece ToStringPiece() const;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(StringFile);
};

}  // namespace unix_file

#endif  // ART_RUNTIME_BASE_UNIX_FILE_STRING_FILE_H_
