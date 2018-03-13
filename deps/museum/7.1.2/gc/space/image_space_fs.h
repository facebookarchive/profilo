/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FS_H_
#define ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FS_H_

#include <dirent.h>
#include <dlfcn.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "base/unix_file/fd_file.h"
#include "globals.h"
#include "os.h"
#include "runtime.h"
#include "utils.h"

namespace art {
namespace gc {
namespace space {

// This file contains helper code for ImageSpace. It has most of the file-system
// related code, including handling A/B OTA.

namespace impl {

// Delete the directory and its (regular or link) contents. If the recurse flag is true, delete
// sub-directories recursively.
static void DeleteDirectoryContents(const std::string& dir, bool recurse) {
  if (!OS::DirectoryExists(dir.c_str())) {
    return;
  }
  DIR* c_dir = opendir(dir.c_str());
  if (c_dir == nullptr) {
    PLOG(WARNING) << "Unable to open " << dir << " to delete it's contents";
    return;
  }

  for (struct dirent* de = readdir(c_dir); de != nullptr; de = readdir(c_dir)) {
    const char* name = de->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }
    // We only want to delete regular files and symbolic links.
    std::string file = StringPrintf("%s/%s", dir.c_str(), name);
    if (de->d_type != DT_REG && de->d_type != DT_LNK) {
      if (de->d_type == DT_DIR) {
        if (recurse) {
          DeleteDirectoryContents(file, recurse);
          // Try to rmdir the directory.
          if (rmdir(file.c_str()) != 0) {
            PLOG(ERROR) << "Unable to rmdir " << file;
          }
        }
      } else {
        LOG(WARNING) << "Unexpected file type of " << std::hex << de->d_type << " encountered.";
      }
    } else {
      // Try to unlink the file.
      if (unlink(file.c_str()) != 0) {
        PLOG(ERROR) << "Unable to unlink " << file;
      }
    }
  }
  CHECK_EQ(0, closedir(c_dir)) << "Unable to close directory.";
}

}  // namespace impl


// We are relocating or generating the core image. We should get rid of everything. It is all
// out-of-date. We also don't really care if this fails since it is just a convenience.
// Adapted from prune_dex_cache(const char* subdir) in frameworks/native/cmds/installd/commands.c
// Note this should only be used during first boot.
static void PruneDalvikCache(InstructionSet isa) {
  CHECK_NE(isa, kNone);
  // Prune the base /data/dalvik-cache.
  impl::DeleteDirectoryContents(GetDalvikCacheOrDie(".", false), false);
  // Prune /data/dalvik-cache/<isa>.
  impl::DeleteDirectoryContents(GetDalvikCacheOrDie(GetInstructionSetString(isa), false), false);

  // Be defensive. There should be a runtime created here, but this may be called in a test.
  if (Runtime::Current() != nullptr) {
    Runtime::Current()->SetPrunedDalvikCache(true);
  }
}

// We write out an empty file to the zygote's ISA specific cache dir at the start of
// every zygote boot and delete it when the boot completes. If we find a file already
// present, it usually means the boot didn't complete. We wipe the entire dalvik
// cache if that's the case.
static void MarkZygoteStart(const InstructionSet isa, const uint32_t max_failed_boots) {
  const std::string isa_subdir = GetDalvikCacheOrDie(GetInstructionSetString(isa), false);
  const std::string boot_marker = isa_subdir + "/.booting";
  const char* file_name = boot_marker.c_str();

  uint32_t num_failed_boots = 0;
  std::unique_ptr<File> file(OS::OpenFileReadWrite(file_name));
  if (file.get() == nullptr) {
    file.reset(OS::CreateEmptyFile(file_name));

    if (file.get() == nullptr) {
      int saved_errno = errno;
      PLOG(WARNING) << "Failed to create boot marker.";
      if (saved_errno != ENOSPC) {
        return;
      }

      LOG(WARNING) << "Pruning dalvik cache because of low-memory situation.";
      impl::DeleteDirectoryContents(isa_subdir, false);

      // Try once more.
      file.reset(OS::OpenFileReadWrite(file_name));
      if (file == nullptr) {
        PLOG(WARNING) << "Failed to create boot marker.";
        return;
      }
    }
  } else {
    if (!file->ReadFully(&num_failed_boots, sizeof(num_failed_boots))) {
      PLOG(WARNING) << "Failed to read boot marker.";
      file->Erase();
      return;
    }
  }

  if (max_failed_boots != 0 && num_failed_boots > max_failed_boots) {
    LOG(WARNING) << "Incomplete boot detected. Pruning dalvik cache";
    impl::DeleteDirectoryContents(isa_subdir, false);
  }

  ++num_failed_boots;
  VLOG(startup) << "Number of failed boots on : " << boot_marker << " = " << num_failed_boots;

  if (lseek(file->Fd(), 0, SEEK_SET) == -1) {
    PLOG(WARNING) << "Failed to write boot marker.";
    file->Erase();
    return;
  }

  if (!file->WriteFully(&num_failed_boots, sizeof(num_failed_boots))) {
    PLOG(WARNING) << "Failed to write boot marker.";
    file->Erase();
    return;
  }

  if (file->FlushCloseOrErase() != 0) {
    PLOG(WARNING) << "Failed to flush boot marker.";
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FS_H_
