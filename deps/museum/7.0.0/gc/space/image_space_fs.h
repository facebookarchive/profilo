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

static bool HasContent(const char* dir) {
  if (!OS::DirectoryExists(dir)) {
    return false;
  }
  DIR* c_dir = opendir(dir);
  if (c_dir == nullptr) {
    PLOG(WARNING) << "Unable to open " << dir << " to delete it if empty";
    return false;
  }

  for (struct dirent* de = readdir(c_dir); de != nullptr; de = readdir(c_dir)) {
    const char* name = de->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }
    // Something here.
    CHECK_EQ(0, closedir(c_dir)) << "Unable to close directory.";
    return true;
  }
  CHECK_EQ(0, closedir(c_dir)) << "Unable to close directory.";
  return false;
}

// Delete this directory, if empty. Then repeat with the parents. Skips non-existing directories.
// If stop_at isn't null, the recursion will stop when a directory with the given name is found.
static void DeleteEmptyDirectoriesUpTo(const std::string& dir, const char* stop_at) {
  if (HasContent(dir.c_str())) {
    return;
  }
  if (stop_at != nullptr) {
    // This check isn't precise, but good enough in practice.
    if (EndsWith(dir, stop_at)) {
      return;
    }
  }
  if (OS::DirectoryExists(dir.c_str())) {
    if (rmdir(dir.c_str()) != 0) {
      PLOG(ERROR) << "Unable to rmdir " << dir;
      return;
    }
  }
  size_t last_slash = dir.rfind('/');
  if (last_slash != std::string::npos) {
    DeleteEmptyDirectoriesUpTo(dir.substr(0, last_slash), stop_at);
  }
}

static void MoveOTAArtifacts(const char* src, const char* trg) {
  DCHECK(OS::DirectoryExists(src));
  DCHECK(OS::DirectoryExists(trg));

  if (HasContent(trg)) {
    LOG(WARNING) << "We do not support merging caches, but the target isn't empty: " << src
                 << " to " << trg;
    return;
  }

  if (rename(src, trg) != 0) {
    PLOG(ERROR) << "Could not rename OTA cache " << src << " to target " << trg;
  }
}

// This is some dlopen/dlsym and hardcoded data to avoid a dependency on libselinux. Make sure
// this stays in sync!
static bool RelabelOTAFiles(const std::string& dalvik_cache_dir) {
  // We only expect selinux on devices. Don't even attempt this on the host.
  if (!kIsTargetBuild) {
    return true;
  }

  // Custom deleter, so we can use std::unique_ptr.
  struct HandleDeleter {
    void operator()(void* in) {
      if (in != nullptr && dlclose(in) != 0) {
        PLOG(ERROR) << "Could not close selinux handle.";
      }
    }
  };

  // Look for selinux library.
  std::unique_ptr<void, HandleDeleter> selinux_handle(dlopen("libselinux.so", RTLD_NOW));
  if (selinux_handle == nullptr) {
    // Assume everything's OK if we can't open the library.
    return true;
  }
  dlerror();  // Clean dlerror string.

  void* restorecon_ptr = dlsym(selinux_handle.get(), "selinux_android_restorecon");
  if (restorecon_ptr == nullptr) {
    // Can't find the relabel function. That's bad. Make sure the zygote fails, as we have no
    // other recourse to make this error obvious.
    const char* error_string = dlerror();
    LOG(FATAL) << "Could not find selinux restorecon function: "
               << ((error_string != nullptr) ? error_string : "(unknown error)");
    UNREACHABLE();
  }

  using RestoreconFn = int (*)(const char*, unsigned int);
  constexpr unsigned int kRecursive = 4U;

  RestoreconFn restorecon_fn = reinterpret_cast<RestoreconFn>(restorecon_ptr);
  if (restorecon_fn(dalvik_cache_dir.c_str(), kRecursive) != 0) {
    LOG(ERROR) << "Failed to restorecon " << dalvik_cache_dir;
    return false;
  }

  return true;
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
      PLOG(WARNING) << "Failed to create boot marker.";
      return;
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

static void TryMoveOTAArtifacts(const std::string& cache_filename, bool dalvik_cache_exists) {
  // We really assume here global means /data/dalvik-cache, and we'll inject 'ota.' Make sure
  // that's true.
  CHECK(StartsWith(cache_filename, "/data/dalvik-cache")) << cache_filename;

  // Inject ota subdirectory.
  std::string ota_filename(cache_filename);
  ota_filename = ota_filename.insert(strlen("/data/"), "ota/");
  CHECK(StartsWith(ota_filename, "/data/ota/dalvik-cache")) << ota_filename;

  // See if the file exists.
  if (OS::FileExists(ota_filename.c_str())) {
    VLOG(startup) << "OTA directory does exist, checking for artifacts";

    size_t last_slash = ota_filename.rfind('/');
    CHECK_NE(last_slash, std::string::npos);
    std::string ota_source_dir = ota_filename.substr(0, last_slash);

    // We need the dalvik cache now, really.
    if (dalvik_cache_exists) {
      size_t last_cache_slash = cache_filename.rfind('/');
      DCHECK_NE(last_cache_slash, std::string::npos);
      std::string dalvik_cache_target_dir = cache_filename.substr(0, last_cache_slash);

      // First clean the target cache.
      impl::DeleteDirectoryContents(dalvik_cache_target_dir.c_str(), false);

      // Now move things over.
      impl::MoveOTAArtifacts(ota_source_dir.c_str(), dalvik_cache_target_dir.c_str());

      // Last step: ensure the files have the right selinux label.
      if (!impl::RelabelOTAFiles(dalvik_cache_target_dir)) {
        // This isn't good. We potentially moved files, but they have the wrong label. Delete the
        // files.
        LOG(WARNING) << "Could not relabel files, must delete dalvik-cache.";
        impl::DeleteDirectoryContents(dalvik_cache_target_dir.c_str(), false);
      }
    }

    // Cleanup.
    impl::DeleteDirectoryContents(ota_source_dir.c_str(), true);
    impl::DeleteEmptyDirectoriesUpTo(ota_source_dir, "ota");
  } else {
    VLOG(startup) << "No OTA directory.";
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_FS_H_
