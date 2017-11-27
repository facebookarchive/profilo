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

#ifndef ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_
#define ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_

#include <set>
#include <vector>

#include "atomic.h"
#include "dex_cache_resolved_classes.h"
#include "dex_file.h"
#include "method_reference.h"
#include "safe_map.h"

namespace art {

// TODO: rename file.
/**
 * Profile information in a format suitable to be queried by the compiler and
 * performing profile guided compilation.
 * It is a serialize-friendly format based on information collected by the
 * interpreter (ProfileInfo).
 * Currently it stores only the hot compiled methods.
 */
class ProfileCompilationInfo {
 public:
  static const uint8_t kProfileMagic[];
  static const uint8_t kProfileVersion[];

  // Add the given methods and classes to the current profile object.
  bool AddMethodsAndClasses(const std::vector<MethodReference>& methods,
                            const std::set<DexCacheResolvedClasses>& resolved_classes);
  // Loads profile information from the given file descriptor.
  bool Load(int fd);
  // Merge the data from another ProfileCompilationInfo into the current object.
  bool MergeWith(const ProfileCompilationInfo& info);
  // Saves the profile data to the given file descriptor.
  bool Save(int fd);
  // Loads and merges profile information from the given file into the current
  // object and tries to save it back to disk.
  // If `force` is true then the save will go through even if the given file
  // has bad data or its version does not match. In this cases the profile content
  // is ignored.
  bool MergeAndSave(const std::string& filename, uint64_t* bytes_written, bool force);

  // Returns the number of methods that were profiled.
  uint32_t GetNumberOfMethods() const;
  // Returns the number of resolved classes that were profiled.
  uint32_t GetNumberOfResolvedClasses() const;

  // Returns true if the method reference is present in the profiling info.
  bool ContainsMethod(const MethodReference& method_ref) const;

  // Returns true if the class is present in the profiling info.
  bool ContainsClass(const DexFile& dex_file, uint16_t class_def_idx) const;

  // Dumps all the loaded profile info into a string and returns it.
  // If dex_files is not null then the method indices will be resolved to their
  // names.
  // This is intended for testing and debugging.
  std::string DumpInfo(const std::vector<const DexFile*>* dex_files,
                       bool print_full_dex_location = true) const;

  bool Equals(const ProfileCompilationInfo& other);

  static std::string GetProfileDexFileKey(const std::string& dex_location);

  // Returns the class descriptors for all of the classes in the profiles' class sets.
  // Note the dex location is actually the profile key, the caller needs to call back in to the
  // profile info stuff to generate a map back to the dex location.
  std::set<DexCacheResolvedClasses> GetResolvedClasses() const;

  // Clears the resolved classes from the current object.
  void ClearResolvedClasses();

 private:
  enum ProfileLoadSatus {
    kProfileLoadIOError,
    kProfileLoadVersionMismatch,
    kProfileLoadBadData,
    kProfileLoadSuccess
  };

  struct DexFileData {
    explicit DexFileData(uint32_t location_checksum) : checksum(location_checksum) {}
    uint32_t checksum;
    std::set<uint16_t> method_set;
    std::set<uint16_t> class_set;

    bool operator==(const DexFileData& other) const {
      return checksum == other.checksum && method_set == other.method_set;
    }
  };

  using DexFileToProfileInfoMap = SafeMap<const std::string, DexFileData>;

  DexFileData* GetOrAddDexFileData(const std::string& dex_location, uint32_t checksum);
  bool AddMethodIndex(const std::string& dex_location, uint32_t checksum, uint16_t method_idx);
  bool AddClassIndex(const std::string& dex_location, uint32_t checksum, uint16_t class_idx);
  bool AddResolvedClasses(const DexCacheResolvedClasses& classes);

  // Parsing functionality.

  struct ProfileLineHeader {
    std::string dex_location;
    uint16_t method_set_size;
    uint16_t class_set_size;
    uint32_t checksum;
  };

  // A helper structure to make sure we don't read past our buffers in the loops.
  struct SafeBuffer {
   public:
    explicit SafeBuffer(size_t size) : storage_(new uint8_t[size]) {
      ptr_current_ = storage_.get();
      ptr_end_ = ptr_current_ + size;
    }

    // Reads the content of the descriptor at the current position.
    ProfileLoadSatus FillFromFd(int fd,
                                const std::string& source,
                                /*out*/std::string* error);

    // Reads an uint value (high bits to low bits) and advances the current pointer
    // with the number of bits read.
    template <typename T> T ReadUintAndAdvance();

    // Compares the given data with the content current pointer. If the contents are
    // equal it advances the current pointer by data_size.
    bool CompareAndAdvance(const uint8_t* data, size_t data_size);

    // Get the underlying raw buffer.
    uint8_t* Get() { return storage_.get(); }

   private:
    std::unique_ptr<uint8_t> storage_;
    uint8_t* ptr_current_;
    uint8_t* ptr_end_;
  };

  ProfileLoadSatus LoadInternal(int fd, std::string* error);

  ProfileLoadSatus ReadProfileHeader(int fd,
                                     /*out*/uint16_t* number_of_lines,
                                     /*out*/std::string* error);

  ProfileLoadSatus ReadProfileLineHeader(int fd,
                                         /*out*/ProfileLineHeader* line_header,
                                         /*out*/std::string* error);
  ProfileLoadSatus ReadProfileLine(int fd,
                                   const ProfileLineHeader& line_header,
                                   /*out*/std::string* error);

  bool ProcessLine(SafeBuffer& line_buffer,
                   uint16_t method_set_size,
                   uint16_t class_set_size,
                   uint32_t checksum,
                   const std::string& dex_location);

  friend class ProfileCompilationInfoTest;
  friend class CompilerDriverProfileTest;
  friend class ProfileAssistantTest;

  DexFileToProfileInfoMap info_;
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_OFFLINE_PROFILING_INFO_H_
