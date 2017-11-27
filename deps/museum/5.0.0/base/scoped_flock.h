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

#include "base/macros.h"
#include "os.h"

namespace art {

class ScopedFlock {
 public:
  ScopedFlock();

  // Attempts to acquire an exclusive file lock (see flock(2)) on the file
  // at filename, and blocks until it can do so.
  //
  // Returns true if the lock could be acquired, or false if an error
  // occurred. It is an error if the file does not exist, or if its inode
  // changed (usually due to a new file being created at the same path)
  // between attempts to lock it.
  bool Init(const char* filename, std::string* error_msg);
  // Attempt to acquire an exclusive file lock (see flock(2)) on 'file'.
  // Returns true if the lock could be acquired or false if an error
  // occured.
  bool Init(File* file, std::string* error_msg);

  // Returns the (locked) file associated with this instance.
  File* GetFile();

  // Returns whether a file is held.
  bool HasFile();

  ~ScopedFlock();

 private:
  std::unique_ptr<File> file_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFlock);
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_SCOPED_FLOCK_H_
