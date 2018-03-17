/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_SCOPED_FLOCK_H_
#define ART_RUNTIME_BASE_SCOPED_FLOCK_H_

#include <memory>
#include <string>

#include "android-base/unique_fd.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/unix_file/fd_file.h"
#include "os.h"

namespace art {

class LockedFile;
class LockedFileCloseNoFlush;

// A scoped File object that calls Close without flushing.
typedef std::unique_ptr<LockedFile, LockedFileCloseNoFlush> ScopedFlock;

class LockedFile : public unix_file::FdFile {
 public:
  // Attempts to acquire an exclusive file lock (see flock(2)) on the file
  // at filename, and blocks until it can do so.
  //
  // It is an error if its inode changed (usually due to a new file being
  // created at the same path) between attempts to lock it. In blocking mode,
  // locking will be retried if the file changed. In non-blocking mode, false
  // is returned and no attempt is made to re-acquire the lock.
  //
  // The file is opened with the provided flags.
  static ScopedFlock Open(const char* filename, int flags, bool block,
                          std::string* error_msg);

  // Calls Open(filename, O_CREAT | O_RDWR, true, errror_msg)
  static ScopedFlock Open(const char* filename, std::string* error_msg);

  // Attempt to acquire an exclusive file lock (see flock(2)) on 'file'.
  // Returns true if the lock could be acquired or false if an error
  // occured.
  static ScopedFlock DupOf(const int fd, const std::string& path,
                           const bool read_only_mode, std::string* error_message);

  // Release a lock held on this file, if any.
  void ReleaseLock();

 private:
  // Constructors should not be invoked directly, use one of the factory
  // methods instead.
  explicit LockedFile(FdFile&& other) : FdFile(std::move(other)) {
  }

  // Constructors should not be invoked directly, use one of the factory
  // methods instead.
  LockedFile(int fd, const std::string& path, bool check_usage, bool read_only_mode)
      : FdFile(fd, path, check_usage, read_only_mode) {
  }
};

class LockedFileCloseNoFlush {
 public:
  void operator()(LockedFile* ptr) {
    ptr->ReleaseLock();
    UNUSED(ptr->Close());

    delete ptr;
  }
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_SCOPED_FLOCK_H_
