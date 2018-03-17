/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_OAT_FILE_MANAGER_H_
#define ART_RUNTIME_OAT_FILE_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/mutex.h"
#include "compiler_filter.h"
#include "jni.h"

namespace art {

namespace gc {
namespace space {
class ImageSpace;
}  // namespace space
}  // namespace gc

class DexFile;
class OatFile;

// Class for dealing with oat file management.
//
// This class knows about all the loaded oat files and provides utility functions. The oat file
// pointers returned from functions are always valid.
class OatFileManager {
 public:
  OatFileManager() : have_non_pic_oat_file_(false) {}
  ~OatFileManager();

  // Add an oat file to the internal accounting, std::aborts if there already exists an oat file
  // with the same base address. Returns the oat file pointer from oat_file.
  const OatFile* RegisterOatFile(std::unique_ptr<const OatFile> oat_file)
      REQUIRES(!Locks::oat_file_manager_lock_);

  void UnRegisterAndDeleteOatFile(const OatFile* oat_file)
      REQUIRES(!Locks::oat_file_manager_lock_);

  // Find the first opened oat file with the same location, returns null if there are none.
  const OatFile* FindOpenedOatFileFromOatLocation(const std::string& oat_location) const
      REQUIRES(!Locks::oat_file_manager_lock_);

  // Find the oat file which contains a dex files with the given dex base location,
  // returns null if there are none.
  const OatFile* FindOpenedOatFileFromDexLocation(const std::string& dex_base_location) const
      REQUIRES(!Locks::oat_file_manager_lock_);

  // Returns true if we have a non pic oat file.
  bool HaveNonPicOatFile() const {
    return have_non_pic_oat_file_;
  }

  // Returns the boot image oat files.
  std::vector<const OatFile*> GetBootOatFiles() const;

  // Returns the first non-image oat file in the class path.
  const OatFile* GetPrimaryOatFile() const REQUIRES(!Locks::oat_file_manager_lock_);

  // Returns the oat files for the images, registers the oat files.
  // Takes ownership of the imagespace's underlying oat files.
  std::vector<const OatFile*> RegisterImageOatFiles(std::vector<gc::space::ImageSpace*> spaces)
      REQUIRES(!Locks::oat_file_manager_lock_);

  // Finds or creates the oat file holding dex_location. Then loads and returns
  // all corresponding dex files (there may be more than one dex file loaded
  // in the case of multidex).
  // This may return the original, unquickened dex files if the oat file could
  // not be generated.
  //
  // Returns an empty vector if the dex files could not be loaded. In this
  // case, there will be at least one error message returned describing why no
  // dex files could not be loaded. The 'error_msgs' argument must not be
  // null, regardless of whether there is an error or not.
  //
  // This method should not be called with the mutator_lock_ held, because it
  // could end up starving GC if we need to generate or relocate any oat
  // files.
  std::vector<std::unique_ptr<const DexFile>> OpenDexFilesFromOat(
      const char* dex_location,
      const char* oat_location,
      jobject class_loader,
      jobjectArray dex_elements,
      /*out*/ const OatFile** out_oat_file,
      /*out*/ std::vector<std::string>* error_msgs)
      REQUIRES(!Locks::oat_file_manager_lock_, !Locks::mutator_lock_);

  void DumpForSigQuit(std::ostream& os);

  static void SetCompilerFilter(CompilerFilter::Filter filter) {
    filter_ = filter;
  }

 private:
  // Check that the shared libraries in the given oat file match those in the given class loader and
  // dex elements. If the class loader is null or we do not support one of the class loaders in the
  // chain, compare against all non-boot oat files instead. If the shared libraries are not ok,
  // check for duplicate class definitions of the given oat file against the oat files (either from
  // the class loader and dex elements if possible or all non-boot oat files otherwise).
  // Return true if there are any class definition collisions in the oat_file.
  bool HasCollisions(const OatFile* oat_file,
                     jobject class_loader,
                     jobjectArray dex_elements,
                     /*out*/ std::string* error_msg) const
      REQUIRES(!Locks::oat_file_manager_lock_);

  const OatFile* FindOpenedOatFileFromOatLocationLocked(const std::string& oat_location) const
      REQUIRES(Locks::oat_file_manager_lock_);

  std::set<std::unique_ptr<const OatFile>> oat_files_ GUARDED_BY(Locks::oat_file_manager_lock_);
  bool have_non_pic_oat_file_;

  // The compiler filter used for oat files loaded by the oat file manager.
  static CompilerFilter::Filter filter_;

  DISALLOW_COPY_AND_ASSIGN(OatFileManager);
};

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_MANAGER_H_
