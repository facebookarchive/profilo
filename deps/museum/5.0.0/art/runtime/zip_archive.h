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

#ifndef ART_RUNTIME_ZIP_ARCHIVE_H_
#define ART_RUNTIME_ZIP_ARCHIVE_H_

#include <stdint.h>
#include <ziparchive/zip_archive.h>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/unix_file/random_access_file.h"
#include "globals.h"
#include "mem_map.h"
#include "os.h"
#include "safe_map.h"

namespace art {

class ZipArchive;
class MemMap;

class ZipEntry {
 public:
  bool ExtractToFile(File& file, std::string* error_msg);
  MemMap* ExtractToMemMap(const char* zip_filename, const char* entry_filename,
                          std::string* error_msg);
  virtual ~ZipEntry();

  uint32_t GetUncompressedLength();
  uint32_t GetCrc32();

 private:
  ZipEntry(ZipArchiveHandle handle,
           ::ZipEntry* zip_entry) : handle_(handle), zip_entry_(zip_entry) {}

  ZipArchiveHandle handle_;
  ::ZipEntry* const zip_entry_;

  friend class ZipArchive;
  DISALLOW_COPY_AND_ASSIGN(ZipEntry);
};

class ZipArchive {
 public:
  // return new ZipArchive instance on success, NULL on error.
  static ZipArchive* Open(const char* filename, std::string* error_msg);
  static ZipArchive* OpenFromFd(int fd, const char* filename, std::string* error_msg);

  ZipEntry* Find(const char* name, std::string* error_msg) const;

  ~ZipArchive() {
    CloseArchive(handle_);
  }

 private:
  explicit ZipArchive(ZipArchiveHandle handle) : handle_(handle) {}

  friend class ZipEntry;

  ZipArchiveHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(ZipArchive);
};

}  // namespace art

#endif  // ART_RUNTIME_ZIP_ARCHIVE_H_
