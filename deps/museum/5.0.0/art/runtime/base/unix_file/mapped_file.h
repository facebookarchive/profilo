/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_UNIX_FILE_MAPPED_FILE_H_
#define ART_RUNTIME_BASE_UNIX_FILE_MAPPED_FILE_H_

#include <fcntl.h>
#include <string>
#include "base/unix_file/fd_file.h"

namespace unix_file {

// Random access file which handles an mmap(2), munmap(2) pair in C++
// RAII style. When a file is mmapped, the random access file
// interface accesses the mmapped memory directly; otherwise, the
// standard file I/O is used. Whenever a function fails, it returns
// false and errno is set to the corresponding error code.
class MappedFile : public FdFile {
 public:
  // File modes used in Open().
  enum FileMode {
#ifdef __linux__
    kReadOnlyMode = O_RDONLY | O_LARGEFILE,
    kReadWriteMode = O_CREAT | O_RDWR | O_LARGEFILE,
#else
    kReadOnlyMode = O_RDONLY,
    kReadWriteMode = O_CREAT | O_RDWR,
#endif
  };

  MappedFile() : FdFile(), file_size_(-1), mapped_file_(NULL) {
  }
  // Creates a MappedFile using the given file descriptor. Takes ownership of
  // the file descriptor.
  explicit MappedFile(int fd) : FdFile(fd), file_size_(-1), mapped_file_(NULL) {
  }

  // Unmaps and closes the file if needed.
  virtual ~MappedFile();

  // Maps an opened file to memory in the read-only mode.
  bool MapReadOnly();

  // Maps an opened file to memory in the read-write mode. Before the
  // file is mapped, it is truncated to 'file_size' bytes.
  bool MapReadWrite(int64_t file_size);

  // Unmaps a mapped file so that, e.g., SetLength() may be invoked.
  bool Unmap();

  // RandomAccessFile API.
  // The functions below require that the file is open, but it doesn't
  // have to be mapped.
  virtual int Close();
  virtual int64_t Read(char* buf, int64_t byte_count, int64_t offset) const;
  // SetLength() requires that the file is not mmapped.
  virtual int SetLength(int64_t new_length);
  virtual int64_t GetLength() const;
  virtual int Flush();
  // Write() requires that, if the file is mmapped, it is mmapped in
  // the read-write mode. Writes past the end of file are discarded.
  virtual int64_t Write(const char* buf, int64_t byte_count, int64_t offset);

  // A convenience method equivalent to GetLength().
  int64_t size() const;

  // Returns true if the file has been mmapped.
  bool IsMapped() const;

  // Returns a pointer to the start of the memory mapping once the
  // file is successfully mapped; crashes otherwise.
  char* data() const;

 private:
  enum MapMode {
    kMapReadOnly = 1,
    kMapReadWrite = 2,
  };

  mutable int64_t file_size_;  // May be updated in GetLength().
  void* mapped_file_;
  MapMode map_mode_;

  DISALLOW_COPY_AND_ASSIGN(MappedFile);
};

}  // namespace unix_file

#endif  // ART_RUNTIME_BASE_UNIX_FILE_MAPPED_FILE_H_
