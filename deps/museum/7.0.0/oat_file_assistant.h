/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_OAT_FILE_ASSISTANT_H_
#define ART_RUNTIME_OAT_FILE_ASSISTANT_H_

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include "arch/instruction_set.h"
#include "base/scoped_flock.h"
#include "base/unix_file/fd_file.h"
#include "compiler_filter.h"
#include "oat_file.h"
#include "os.h"
#include "profiler.h"

namespace art {

namespace gc {
namespace space {
class ImageSpace;
}  // namespace space
}  // namespace gc

// Class for assisting with oat file management.
//
// This class collects common utilities for determining the status of an oat
// file on the device, updating the oat file, and loading the oat file.
//
// The oat file assistant is intended to be used with dex locations not on the
// boot class path. See the IsInBootClassPath method for a way to check if the
// dex location is in the boot class path.
class OatFileAssistant {
 public:
  enum DexOptNeeded {
    // kNoDexOptNeeded - The code for this dex location is up to date and can
    // be used as is.
    // Matches Java: dalvik.system.DexFile.NO_DEXOPT_NEEDED = 0
    kNoDexOptNeeded = 0,

    // kDex2OatNeeded - In order to make the code for this dex location up to
    // date, dex2oat must be run on the dex file.
    // Matches Java: dalvik.system.DexFile.DEX2OAT_NEEDED = 1
    kDex2OatNeeded = 1,

    // kPatchOatNeeded - In order to make the code for this dex location up to
    // date, patchoat must be run on the odex file.
    // Matches Java: dalvik.system.DexFile.PATCHOAT_NEEDED = 2
    kPatchOatNeeded = 2,

    // kSelfPatchOatNeeded - In order to make the code for this dex location
    // up to date, patchoat must be run on the oat file.
    // Matches Java: dalvik.system.DexFile.SELF_PATCHOAT_NEEDED = 3
    kSelfPatchOatNeeded = 3,
  };

  enum OatStatus {
    // kOatOutOfDate - An oat file is said to be out of date if the file does
    // not exist, is out of date with respect to the dex file or boot image,
    // or does not meet the target compilation type.
    kOatOutOfDate,

    // kOatNeedsRelocation - An oat file is said to need relocation if the
    // code is up to date, but not yet properly relocated for address space
    // layout randomization (ASLR). In this case, the oat file is neither
    // "out of date" nor "up to date".
    kOatNeedsRelocation,

    // kOatUpToDate - An oat file is said to be up to date if it is not out of
    // date and has been properly relocated for the purposes of ASLR.
    kOatUpToDate,
  };

  // Constructs an OatFileAssistant object to assist the oat file
  // corresponding to the given dex location with the target instruction set.
  //
  // The dex_location must not be null and should remain available and
  // unchanged for the duration of the lifetime of the OatFileAssistant object.
  // Typically the dex_location is the absolute path to the original,
  // un-optimized dex file.
  //
  // Note: Currently the dex_location must have an extension.
  // TODO: Relax this restriction?
  //
  // The isa should be either the 32 bit or 64 bit variant for the current
  // device. For example, on an arm device, use arm or arm64. An oat file can
  // be loaded executable only if the ISA matches the current runtime.
  //
  // profile_changed should be true if the profile has recently changed
  // for this dex location.
  //
  // load_executable should be true if the caller intends to try and load
  // executable code for this dex location.
  OatFileAssistant(const char* dex_location,
                   const InstructionSet isa,
                   bool profile_changed,
                   bool load_executable);

  // Constructs an OatFileAssistant, providing an explicit target oat_location
  // to use instead of the standard oat location.
  OatFileAssistant(const char* dex_location,
                   const char* oat_location,
                   const InstructionSet isa,
                   bool profile_changed,
                   bool load_executable);

  ~OatFileAssistant();

  // Returns true if the dex location refers to an element of the boot class
  // path.
  bool IsInBootClassPath();

  // Obtains a lock on the target oat file.
  // Only one OatFileAssistant object can hold the lock for a target oat file
  // at a time. The Lock is released automatically when the OatFileAssistant
  // object goes out of scope. The Lock() method must not be called if the
  // lock has already been acquired.
  //
  // Returns true on success.
  // Returns false on error, in which case error_msg will contain more
  // information on the error.
  //
  // The 'error_msg' argument must not be null.
  //
  // This is intended to be used to avoid race conditions when multiple
  // processes generate oat files, such as when a foreground Activity and
  // a background Service both use DexClassLoaders pointing to the same dex
  // file.
  bool Lock(std::string* error_msg);

  // Return what action needs to be taken to produce up-to-date code for this
  // dex location that is at least as good as an oat file generated with the
  // given compiler filter.
  DexOptNeeded GetDexOptNeeded(CompilerFilter::Filter target_compiler_filter);

  // Returns true if there is up-to-date code for this dex location,
  // irrespective of the compiler filter of the up-to-date code.
  bool IsUpToDate();

  // Return code used when attempting to generate updated code.
  enum ResultOfAttemptToUpdate {
    kUpdateFailed,        // We tried making the code up to date, but
                          // encountered an unexpected failure.
    kUpdateNotAttempted,  // We wanted to update the code, but determined we
                          // should not make the attempt.
    kUpdateSucceeded      // We successfully made the code up to date
                          // (possibly by doing nothing).
  };

  // Attempts to generate or relocate the oat file as needed to make it up to
  // date with in a way that is at least as good as an oat file generated with
  // the given compiler filter.
  // Returns the result of attempting to update the code.
  //
  // If the result is not kUpdateSucceeded, the value of error_msg will be set
  // to a string describing why there was a failure or the update was not
  // attempted. error_msg must not be null.
  ResultOfAttemptToUpdate MakeUpToDate(CompilerFilter::Filter target_compiler_filter,
                                       std::string* error_msg);

  // Returns an oat file that can be used for loading dex files.
  // Returns null if no suitable oat file was found.
  //
  // After this call, no other methods of the OatFileAssistant should be
  // called, because access to the loaded oat file has been taken away from
  // the OatFileAssistant object.
  std::unique_ptr<OatFile> GetBestOatFile();

  // Open and returns an image space associated with the oat file.
  gc::space::ImageSpace* OpenImageSpace(const OatFile* oat_file);

  // Loads the dex files in the given oat file for the given dex location.
  // The oat file should be up to date for the given dex location.
  // This loads multiple dex files in the case of multidex.
  // Returns an empty vector if no dex files for that location could be loaded
  // from the oat file.
  //
  // The caller is responsible for freeing the dex_files returned, if any. The
  // dex_files will only remain valid as long as the oat_file is valid.
  static std::vector<std::unique_ptr<const DexFile>> LoadDexFiles(
      const OatFile& oat_file, const char* dex_location);

  // Returns true if there are dex files in the original dex location that can
  // be compiled with dex2oat for this dex location.
  // Returns false if there is no original dex file, or if the original dex
  // file is an apk/zip without a classes.dex entry.
  bool HasOriginalDexFiles();

  // If the dex file has been installed with a compiled oat file alongside
  // it, the compiled oat file will have the extension .odex, and is referred
  // to as the odex file. It is called odex for legacy reasons; the file is
  // really an oat file. The odex file will often, but not always, have a
  // patch delta of 0 and need to be relocated before use for the purposes of
  // ASLR. The odex file is treated as if it were read-only.
  // These methods return the location and status of the odex file for the dex
  // location.
  // Notes:
  //  * OdexFileName may return null if the odex file name could not be
  //    determined.
  const std::string* OdexFileName();
  bool OdexFileExists();
  OatStatus OdexFileStatus();
  bool OdexFileIsOutOfDate();
  bool OdexFileNeedsRelocation();
  bool OdexFileIsUpToDate();
  // Must only be called if the associated odex file exists, i.e, if
  // |OdexFileExists() == true|.
  CompilerFilter::Filter OdexFileCompilerFilter();

  // When the dex files is compiled on the target device, the oat file is the
  // result. The oat file will have been relocated to some
  // (possibly-out-of-date) offset for ASLR.
  // These methods return the location and status of the target oat file for
  // the dex location.
  //
  // Notes:
  //  * OatFileName may return null if the oat file name could not be
  //    determined.
  const std::string* OatFileName();
  bool OatFileExists();
  OatStatus OatFileStatus();
  bool OatFileIsOutOfDate();
  bool OatFileNeedsRelocation();
  bool OatFileIsUpToDate();
  // Must only be called if the associated oat file exists, i.e, if
  // |OatFileExists() == true|.
  CompilerFilter::Filter OatFileCompilerFilter();

  // Return image file name. Does not cache since it relies on the oat file.
  std::string ArtFileName(const OatFile* oat_file) const;

  // These methods return the status for a given opened oat file with respect
  // to the dex location.
  OatStatus GivenOatFileStatus(const OatFile& file);
  bool GivenOatFileIsOutOfDate(const OatFile& file);
  bool GivenOatFileNeedsRelocation(const OatFile& file);
  bool GivenOatFileIsUpToDate(const OatFile& file);

  // Generates the oat file by relocation from the named input file.
  // This does not check the current status before attempting to relocate the
  // oat file.
  //
  // If the result is not kUpdateSucceeded, the value of error_msg will be set
  // to a string describing why there was a failure or the update was not
  // attempted. error_msg must not be null.
  ResultOfAttemptToUpdate RelocateOatFile(const std::string* input_file, std::string* error_msg);

  // Generate the oat file from the dex file using the given compiler filter.
  // This does not check the current status before attempting to generate the
  // oat file.
  //
  // If the result is not kUpdateSucceeded, the value of error_msg will be set
  // to a string describing why there was a failure or the update was not
  // attempted. error_msg must not be null.
  ResultOfAttemptToUpdate GenerateOatFile(CompilerFilter::Filter filter, std::string* error_msg);

  // Executes dex2oat using the current runtime configuration overridden with
  // the given arguments. This does not check to see if dex2oat is enabled in
  // the runtime configuration.
  // Returns true on success.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be null.
  //
  // TODO: The OatFileAssistant probably isn't the right place to have this
  // function.
  static bool Dex2Oat(const std::vector<std::string>& args, std::string* error_msg);

  // Constructs the odex file name for the given dex location.
  // Returns true on success, in which case odex_filename is set to the odex
  // file name.
  // Returns false on error, in which case error_msg describes the error.
  // Neither odex_filename nor error_msg may be null.
  static bool DexFilenameToOdexFilename(const std::string& location,
      InstructionSet isa, std::string* odex_filename, std::string* error_msg);

  static uint32_t CalculateCombinedImageChecksum(InstructionSet isa = kRuntimeISA);

 private:
  struct ImageInfo {
    uint32_t oat_checksum = 0;
    uintptr_t oat_data_begin = 0;
    int32_t patch_delta = 0;
    std::string location;
  };

  // Returns the path to the dalvik cache directory.
  // Does not check existence of the cache or try to create it.
  // Includes the trailing slash.
  // Returns an empty string if we can't get the dalvik cache directory path.
  std::string DalvikCacheDirectory();

  // Returns the current image location.
  // Returns an empty string if the image location could not be retrieved.
  //
  // TODO: This method should belong with an image file manager, not
  // the oat file assistant.
  static std::string ImageLocation();

  // Gets the dex checksum required for an up-to-date oat file.
  // Returns dex_checksum if a required checksum was located. Returns
  // null if the required checksum was not found.
  // The caller shouldn't clean up or free the returned pointer.
  // This sets the has_original_dex_files_ field to true if a checksum was
  // found for the dex_location_ dex file.
  const uint32_t* GetRequiredDexChecksum();

  // Returns the loaded odex file.
  // Loads the file if needed. Returns null if the file failed to load.
  // The caller shouldn't clean up or free the returned pointer.
  const OatFile* GetOdexFile();

  // Returns true if the compiler filter used to generate the odex file is at
  // least as good as the given target filter.
  bool OdexFileCompilerFilterIsOkay(CompilerFilter::Filter target);

  // Returns true if the odex file is opened executable.
  bool OdexFileIsExecutable();

  // Returns true if the odex file has patch info required to run patchoat.
  bool OdexFileHasPatchInfo();

  // Clear any cached information about the odex file that depends on the
  // contents of the file.
  void ClearOdexFileCache();

  // Returns the loaded oat file.
  // Loads the file if needed. Returns null if the file failed to load.
  // The caller shouldn't clean up or free the returned pointer.
  const OatFile* GetOatFile();

  // Returns true if the compiler filter used to generate the oat file is at
  // least as good as the given target filter.
  bool OatFileCompilerFilterIsOkay(CompilerFilter::Filter target);

  // Returns true if the oat file is opened executable.
  bool OatFileIsExecutable();

  // Returns true if the oat file has patch info required to run patchoat.
  bool OatFileHasPatchInfo();

  // Clear any cached information about the oat file that depends on the
  // contents of the file.
  void ClearOatFileCache();

  // Returns the loaded image info.
  // Loads the image info if needed. Returns null if the image info failed
  // to load.
  // The caller shouldn't clean up or free the returned pointer.
  const ImageInfo* GetImageInfo();

  uint32_t GetCombinedImageChecksum();

  // To implement Lock(), we lock a dummy file where the oat file would go
  // (adding ".flock" to the target file name) and retain the lock for the
  // remaining lifetime of the OatFileAssistant object.
  ScopedFlock flock_;

  std::string dex_location_;

  // In a properly constructed OatFileAssistant object, isa_ should be either
  // the 32 or 64 bit variant for the current device.
  const InstructionSet isa_ = kNone;

  // Whether the profile has recently changed.
  bool profile_changed_ = false;

  // Whether we will attempt to load oat files executable.
  bool load_executable_ = false;

  // Cached value of the required dex checksum.
  // This should be accessed only by the GetRequiredDexChecksum() method.
  uint32_t cached_required_dex_checksum_;
  bool required_dex_checksum_attempted_ = false;
  bool required_dex_checksum_found_;
  bool has_original_dex_files_;

  // Cached value of the odex file name.
  // This should be accessed only by the OdexFileName() method.
  bool cached_odex_file_name_attempted_ = false;
  bool cached_odex_file_name_found_;
  std::string cached_odex_file_name_;

  // Cached value of the loaded odex file.
  // Use the GetOdexFile method rather than accessing this directly, unless you
  // know the odex file isn't out of date.
  bool odex_file_load_attempted_ = false;
  std::unique_ptr<OatFile> cached_odex_file_;

  // Cached results for OdexFileIsOutOfDate
  bool odex_file_is_out_of_date_attempted_ = false;
  bool cached_odex_file_is_out_of_date_;

  // Cached results for OdexFileIsUpToDate
  bool odex_file_is_up_to_date_attempted_ = false;
  bool cached_odex_file_is_up_to_date_;

  // Cached value of the oat file name.
  // This should be accessed only by the OatFileName() method.
  bool cached_oat_file_name_attempted_ = false;
  bool cached_oat_file_name_found_;
  std::string cached_oat_file_name_;

  // Cached value of the loaded oat file.
  // Use the GetOatFile method rather than accessing this directly, unless you
  // know the oat file isn't out of date.
  bool oat_file_load_attempted_ = false;
  std::unique_ptr<OatFile> cached_oat_file_;

  // Cached results for OatFileIsOutOfDate
  bool oat_file_is_out_of_date_attempted_ = false;
  bool cached_oat_file_is_out_of_date_;

  // Cached results for OatFileIsUpToDate
  bool oat_file_is_up_to_date_attempted_ = false;
  bool cached_oat_file_is_up_to_date_;

  // Cached value of the image info.
  // Use the GetImageInfo method rather than accessing these directly.
  // TODO: The image info should probably be moved out of the oat file
  // assistant to an image file manager.
  bool image_info_load_attempted_ = false;
  bool image_info_load_succeeded_ = false;
  ImageInfo cached_image_info_;
  uint32_t combined_image_checksum_ = 0;

  // For debugging only.
  // If this flag is set, the oat or odex file has been released to the user
  // of the OatFileAssistant object and the OatFileAssistant object is in a
  // bad state and should no longer be used.
  bool oat_file_released_ = false;

  DISALLOW_COPY_AND_ASSIGN(OatFileAssistant);
};

std::ostream& operator << (std::ostream& stream, const OatFileAssistant::OatStatus status);

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_ASSISTANT_H_
