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

#ifndef ART_RUNTIME_JIT_PROFILE_COMPILATION_INFO_H_
#define ART_RUNTIME_JIT_PROFILE_COMPILATION_INFO_H_

#include <set>
#include <vector>

#include "atomic.h"
#include "base/arena_object.h"
#include "base/arena_containers.h"
#include "dex_cache_resolved_classes.h"
#include "dex_file.h"
#include "dex_file_types.h"
#include "method_reference.h"
#include "safe_map.h"

namespace art {

/**
 *  Convenient class to pass around profile information (including inline caches)
 *  without the need to hold GC-able objects.
 */
struct ProfileMethodInfo {
  struct ProfileClassReference {
    ProfileClassReference() : dex_file(nullptr) {}
    ProfileClassReference(const DexFile* dex, const dex::TypeIndex index)
        : dex_file(dex), type_index(index) {}

    const DexFile* dex_file;
    dex::TypeIndex type_index;
  };

  struct ProfileInlineCache {
    ProfileInlineCache(uint32_t pc,
                       bool missing_types,
                       const std::vector<ProfileClassReference>& profile_classes)
        : dex_pc(pc), is_missing_types(missing_types), classes(profile_classes) {}

    const uint32_t dex_pc;
    const bool is_missing_types;
    const std::vector<ProfileClassReference> classes;
  };

  ProfileMethodInfo(const DexFile* dex, uint32_t method_index)
      : dex_file(dex), dex_method_index(method_index) {}

  ProfileMethodInfo(const DexFile* dex,
                    uint32_t method_index,
                    const std::vector<ProfileInlineCache>& caches)
      : dex_file(dex), dex_method_index(method_index), inline_caches(caches) {}

  const DexFile* dex_file;
  const uint32_t dex_method_index;
  const std::vector<ProfileInlineCache> inline_caches;
};

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

  // Data structures for encoding the offline representation of inline caches.
  // This is exposed as public in order to make it available to dex2oat compilations
  // (see compiler/optimizing/inliner.cc).

  // A dex location together with its checksum.
  struct DexReference {
    DexReference() : dex_checksum(0) {}

    DexReference(const std::string& location, uint32_t checksum)
        : dex_location(location), dex_checksum(checksum) {}

    bool operator==(const DexReference& other) const {
      return dex_checksum == other.dex_checksum && dex_location == other.dex_location;
    }

    bool MatchesDex(const DexFile* dex_file) const {
      return dex_checksum == dex_file->GetLocationChecksum() &&
           dex_location == GetProfileDexFileKey(dex_file->GetLocation());
    }

    std::string dex_location;
    uint32_t dex_checksum;
  };

  // Encodes a class reference in the profile.
  // The owning dex file is encoded as the index (dex_profile_index) it has in the
  // profile rather than as a full DexRefence(location,checksum).
  // This avoids excessive string copying when managing the profile data.
  // The dex_profile_index is an index in either of:
  //  - OfflineProfileMethodInfo#dex_references vector (public use)
  //  - DexFileData#profile_index (internal use).
  // Note that the dex_profile_index is not necessary the multidex index.
  // We cannot rely on the actual multidex index because a single profile may store
  // data from multiple splits. This means that a profile may contain a classes2.dex from split-A
  // and one from split-B.
  struct ClassReference : public ValueObject {
    ClassReference(uint8_t dex_profile_idx, const dex::TypeIndex type_idx) :
      dex_profile_index(dex_profile_idx), type_index(type_idx) {}

    bool operator==(const ClassReference& other) const {
      return dex_profile_index == other.dex_profile_index && type_index == other.type_index;
    }
    bool operator<(const ClassReference& other) const {
      return dex_profile_index == other.dex_profile_index
          ? type_index < other.type_index
          : dex_profile_index < other.dex_profile_index;
    }

    uint8_t dex_profile_index;  // the index of the owning dex in the profile info
    dex::TypeIndex type_index;  // the type index of the class
  };

  // The set of classes that can be found at a given dex pc.
  using ClassSet = ArenaSet<ClassReference>;

  // Encodes the actual inline cache for a given dex pc (whether or not the receiver is
  // megamorphic and its possible types).
  // If the receiver is megamorphic or is missing types the set of classes will be empty.
  struct DexPcData : public ArenaObject<kArenaAllocProfile> {
    explicit DexPcData(ArenaAllocator* arena)
        : is_missing_types(false),
          is_megamorphic(false),
          classes(std::less<ClassReference>(), arena->Adapter(kArenaAllocProfile)) {}
    void AddClass(uint16_t dex_profile_idx, const dex::TypeIndex& type_idx);
    void SetIsMegamorphic() {
      if (is_missing_types) return;
      is_megamorphic = true;
      classes.clear();
    }
    void SetIsMissingTypes() {
      is_megamorphic = false;
      is_missing_types = true;
      classes.clear();
    }
    bool operator==(const DexPcData& other) const {
      return is_megamorphic == other.is_megamorphic &&
          is_missing_types == other.is_missing_types &&
          classes == other.classes;
    }

    // Not all runtime types can be encoded in the profile. For example if the receiver
    // type is in a dex file which is not tracked for profiling its type cannot be
    // encoded. When types are missing this field will be set to true.
    bool is_missing_types;
    bool is_megamorphic;
    ClassSet classes;
  };

  // The inline cache map: DexPc -> DexPcData.
  using InlineCacheMap = ArenaSafeMap<uint16_t, DexPcData>;

  // Maps a method dex index to its inline cache.
  using MethodMap = ArenaSafeMap<uint16_t, InlineCacheMap>;

  // Encodes the full set of inline caches for a given method.
  // The dex_references vector is indexed according to the ClassReference::dex_profile_index.
  // i.e. the dex file of any ClassReference present in the inline caches can be found at
  // dex_references[ClassReference::dex_profile_index].
  struct OfflineProfileMethodInfo {
    explicit OfflineProfileMethodInfo(const InlineCacheMap* inline_cache_map)
        : inline_caches(inline_cache_map) {}

    bool operator==(const OfflineProfileMethodInfo& other) const;

    const InlineCacheMap* const inline_caches;
    std::vector<DexReference> dex_references;
  };

  // Public methods to create, extend or query the profile.
  ProfileCompilationInfo();
  explicit ProfileCompilationInfo(ArenaPool* arena_pool);

  ~ProfileCompilationInfo();

  // Add the given methods and classes to the current profile object.
  bool AddMethodsAndClasses(const std::vector<ProfileMethodInfo>& methods,
                            const std::set<DexCacheResolvedClasses>& resolved_classes);

  // Load profile information from the given file descriptor.
  // If the current profile is non-empty the load will fail.
  bool Load(int fd);

  // Load profile information from the given file
  // If the current profile is non-empty the load will fail.
  // If clear_if_invalid is true and the file is invalid the method clears the
  // the file and returns true.
  bool Load(const std::string& filename, bool clear_if_invalid);

  // Merge the data from another ProfileCompilationInfo into the current object.
  bool MergeWith(const ProfileCompilationInfo& info);

  // Save the profile data to the given file descriptor.
  bool Save(int fd);

  // Save the current profile into the given file. The file will be cleared before saving.
  bool Save(const std::string& filename, uint64_t* bytes_written);

  // Return the number of methods that were profiled.
  uint32_t GetNumberOfMethods() const;

  // Return the number of resolved classes that were profiled.
  uint32_t GetNumberOfResolvedClasses() const;

  // Return true if the method reference is present in the profiling info.
  bool ContainsMethod(const MethodReference& method_ref) const;

  // Return true if the class's type is present in the profiling info.
  bool ContainsClass(const DexFile& dex_file, dex::TypeIndex type_idx) const;

  // Return the method data for the given location and index from the profiling info.
  // If the method index is not found or the checksum doesn't match, null is returned.
  // Note: the inline cache map is a pointer to the map stored in the profile and
  // its allocation will go away if the profile goes out of scope.
  std::unique_ptr<OfflineProfileMethodInfo> GetMethod(const std::string& dex_location,
                                                      uint32_t dex_checksum,
                                                      uint16_t dex_method_index) const;

  // Dump all the loaded profile info into a string and returns it.
  // If dex_files is not null then the method indices will be resolved to their
  // names.
  // This is intended for testing and debugging.
  std::string DumpInfo(const std::vector<std::unique_ptr<const DexFile>>* dex_files,
                       bool print_full_dex_location = true) const;
  std::string DumpInfo(const std::vector<const DexFile*>* dex_files,
                       bool print_full_dex_location = true) const;

  // Return the classes and methods for a given dex file through out args. The out args are the set
  // of class as well as the methods and their associated inline caches. Returns true if the dex
  // file is register and has a matching checksum, false otherwise.
  bool GetClassesAndMethods(const DexFile& dex_file,
                            /*out*/std::set<dex::TypeIndex>* class_set,
                            /*out*/std::set<uint16_t>* method_set) const;

  // Perform an equality test with the `other` profile information.
  bool Equals(const ProfileCompilationInfo& other);

  // Return the class descriptors for all of the classes in the profiles' class sets.
  std::set<DexCacheResolvedClasses> GetResolvedClasses(
      const std::vector<const DexFile*>& dex_files_) const;

  // Return the profile key associated with the given dex location.
  static std::string GetProfileDexFileKey(const std::string& dex_location);

  // Generate a test profile which will contain a percentage of the total maximum
  // number of methods and classes (method_ratio and class_ratio).
  static bool GenerateTestProfile(int fd,
                                  uint16_t number_of_dex_files,
                                  uint16_t method_ratio,
                                  uint16_t class_ratio,
                                  uint32_t random_seed);

  // Generate a test profile which will randomly contain classes and methods from
  // the provided list of dex files.
  static bool GenerateTestProfile(int fd,
                                  std::vector<std::unique_ptr<const DexFile>>& dex_files,
                                  uint32_t random_seed);

  // Check that the given profile method info contain the same data.
  static bool Equals(const ProfileCompilationInfo::OfflineProfileMethodInfo& pmi1,
                     const ProfileCompilationInfo::OfflineProfileMethodInfo& pmi2);

  ArenaAllocator* GetArena() { return &arena_; }

 private:
  enum ProfileLoadSatus {
    kProfileLoadWouldOverwiteData,
    kProfileLoadIOError,
    kProfileLoadVersionMismatch,
    kProfileLoadBadData,
    kProfileLoadSuccess
  };

  // Internal representation of the profile information belonging to a dex file.
  // Note that we could do without profile_key (the key used to encode the dex
  // file in the profile) and profile_index (the index of the dex file in the
  // profile) fields in this struct because we can infer them from
  // profile_key_map_ and info_. However, it makes the profiles logic much
  // simpler if we have references here as well.
  struct DexFileData : public DeletableArenaObject<kArenaAllocProfile> {
    DexFileData(ArenaAllocator* arena,
                const std::string& key,
                uint32_t location_checksum,
                uint16_t index)
        : arena_(arena),
          profile_key(key),
          profile_index(index),
          checksum(location_checksum),
          method_map(std::less<uint16_t>(), arena->Adapter(kArenaAllocProfile)),
          class_set(std::less<dex::TypeIndex>(), arena->Adapter(kArenaAllocProfile)) {}

    // The arena used to allocate new inline cache maps.
    ArenaAllocator* arena_;
    // The profile key this data belongs to.
    std::string profile_key;
    // The profile index of this dex file (matches ClassReference#dex_profile_index).
    uint8_t profile_index;
    // The dex checksum.
    uint32_t checksum;
    // The methonds' profile information.
    MethodMap method_map;
    // The classes which have been profiled. Note that these don't necessarily include
    // all the classes that can be found in the inline caches reference.
    ArenaSet<dex::TypeIndex> class_set;

    bool operator==(const DexFileData& other) const {
      return checksum == other.checksum && method_map == other.method_map;
    }

    // Find the inline caches of the the given method index. Add an empty entry if
    // no previous data is found.
    InlineCacheMap* FindOrAddMethod(uint16_t method_index);
  };

  // Return the profile data for the given profile key or null if the dex location
  // already exists but has a different checksum
  DexFileData* GetOrAddDexFileData(const std::string& profile_key, uint32_t checksum);

  // Add a method index to the profile (without inline caches).
  bool AddMethodIndex(const std::string& dex_location, uint32_t checksum, uint16_t method_idx);

  // Add a method to the profile using its online representation (containing runtime structures).
  bool AddMethod(const ProfileMethodInfo& pmi);

  // Add a method to the profile using its offline representation.
  // This is mostly used to facilitate testing.
  bool AddMethod(const std::string& dex_location,
                 uint32_t dex_checksum,
                 uint16_t method_index,
                 const OfflineProfileMethodInfo& pmi);

  // Add a class index to the profile.
  bool AddClassIndex(const std::string& dex_location, uint32_t checksum, dex::TypeIndex type_idx);

  // Add all classes from the given dex cache to the the profile.
  bool AddResolvedClasses(const DexCacheResolvedClasses& classes);

  // Search for the given method in the profile.
  // If found, its inline cache map is returned, otherwise the method returns null.
  const InlineCacheMap* FindMethod(const std::string& dex_location,
                                   uint32_t dex_checksum,
                                   uint16_t dex_method_index) const;

  // Encode the known dex_files into a vector. The index of a dex_reference will
  // be the same as the profile index of the dex file (used to encode the ClassReferences).
  void DexFileToProfileIndex(/*out*/std::vector<DexReference>* dex_references) const;

  // Return the dex data associated with the given profile key or null if the profile
  // doesn't contain the key.
  const DexFileData* FindDexData(const std::string& profile_key) const;

  // Checks if the profile is empty.
  bool IsEmpty() const;

  // Parsing functionality.

  // The information present in the header of each profile line.
  struct ProfileLineHeader {
    std::string dex_location;
    uint16_t class_set_size;
    uint32_t method_region_size_bytes;
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
    template <typename T> bool ReadUintAndAdvance(/*out*/ T* value);

    // Compares the given data with the content current pointer. If the contents are
    // equal it advances the current pointer by data_size.
    bool CompareAndAdvance(const uint8_t* data, size_t data_size);

    // Returns true if the buffer has more data to read.
    bool HasMoreData();

    // Get the underlying raw buffer.
    uint8_t* Get() { return storage_.get(); }

   private:
    std::unique_ptr<uint8_t[]> storage_;
    uint8_t* ptr_current_;
    uint8_t* ptr_end_;
  };

  // Entry point for profile loding functionality.
  ProfileLoadSatus LoadInternal(int fd, std::string* error);

  // Read the profile header from the given fd and store the number of profile
  // lines into number_of_dex_files.
  ProfileLoadSatus ReadProfileHeader(int fd,
                                     /*out*/uint8_t* number_of_dex_files,
                                     /*out*/std::string* error);

  // Read the header of a profile line from the given fd.
  ProfileLoadSatus ReadProfileLineHeader(int fd,
                                         /*out*/ProfileLineHeader* line_header,
                                         /*out*/std::string* error);

  // Read individual elements from the profile line header.
  bool ReadProfileLineHeaderElements(SafeBuffer& buffer,
                                     /*out*/uint16_t* dex_location_size,
                                     /*out*/ProfileLineHeader* line_header,
                                     /*out*/std::string* error);

  // Read a single profile line from the given fd.
  ProfileLoadSatus ReadProfileLine(int fd,
                                   uint8_t number_of_dex_files,
                                   const ProfileLineHeader& line_header,
                                   /*out*/std::string* error);

  // Read all the classes from the buffer into the profile `info_` structure.
  bool ReadClasses(SafeBuffer& buffer,
                   uint16_t classes_to_read,
                   const ProfileLineHeader& line_header,
                   /*out*/std::string* error);

  // Read all the methods from the buffer into the profile `info_` structure.
  bool ReadMethods(SafeBuffer& buffer,
                   uint8_t number_of_dex_files,
                   const ProfileLineHeader& line_header,
                   /*out*/std::string* error);

  // Read the inline cache encoding from line_bufer into inline_cache.
  bool ReadInlineCache(SafeBuffer& buffer,
                       uint8_t number_of_dex_files,
                       /*out*/InlineCacheMap* inline_cache,
                       /*out*/std::string* error);

  // Encode the inline cache into the given buffer.
  void AddInlineCacheToBuffer(std::vector<uint8_t>* buffer,
                              const InlineCacheMap& inline_cache);

  // Return the number of bytes needed to encode the profile information
  // for the methods in dex_data.
  uint32_t GetMethodsRegionSize(const DexFileData& dex_data);

  // Group `classes` by their owning dex profile index and put the result in
  // `dex_to_classes_map`.
  void GroupClassesByDex(
      const ClassSet& classes,
      /*out*/SafeMap<uint8_t, std::vector<dex::TypeIndex>>* dex_to_classes_map);

  // Find the data for the dex_pc in the inline cache. Adds an empty entry
  // if no previous data exists.
  DexPcData* FindOrAddDexPc(InlineCacheMap* inline_cache, uint32_t dex_pc);

  friend class ProfileCompilationInfoTest;
  friend class CompilerDriverProfileTest;
  friend class ProfileAssistantTest;
  friend class Dex2oatLayoutTest;

  ArenaPool default_arena_pool_;
  ArenaAllocator arena_;

  // Vector containing the actual profile info.
  // The vector index is the profile index of the dex data and
  // matched DexFileData::profile_index.
  ArenaVector<DexFileData*> info_;

  // Cache mapping profile keys to profile index.
  // This is used to speed up searches since it avoids iterating
  // over the info_ vector when searching by profile key.
  ArenaSafeMap<const std::string, uint8_t> profile_key_map_;
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_PROFILE_COMPILATION_INFO_H_
