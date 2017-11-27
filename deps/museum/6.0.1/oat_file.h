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

#ifndef ART_RUNTIME_OAT_FILE_H_
#define ART_RUNTIME_OAT_FILE_H_

#include <list>
#include <string>
#include <vector>

#include "base/mutex.h"
#include "base/stringpiece.h"
#include "dex_file.h"
#include "invoke_type.h"
#include "mem_map.h"
#include "mirror/class.h"
#include "oat.h"
#include "os.h"

namespace art {

class BitVector;
class ElfFile;
class MemMap;
class OatMethodOffsets;
class OatHeader;
class OatDexFile;

class OatFile FINAL {
 public:
  typedef art::OatDexFile OatDexFile;

  // Opens an oat file contained within the given elf file. This is always opened as
  // non-executable at the moment.
  static OatFile* OpenWithElfFile(ElfFile* elf_file, const std::string& location,
                                  const char* abs_dex_location,
                                  std::string* error_msg);
  // Open an oat file. Returns null on failure.  Requested base can
  // optionally be used to request where the file should be loaded.
  // See the ResolveRelativeEncodedDexLocation for a description of how the
  // abs_dex_location argument is used.
  static OatFile* Open(const std::string& filename,
                       const std::string& location,
                       uint8_t* requested_base,
                       uint8_t* oat_file_begin,
                       bool executable,
                       const char* abs_dex_location,
                       std::string* error_msg);

  // Open an oat file from an already opened File.
  // Does not use dlopen underneath so cannot be used for runtime use
  // where relocations may be required. Currently used from
  // ImageWriter which wants to open a writable version from an existing
  // file descriptor for patching.
  static OatFile* OpenWritable(File* file, const std::string& location,
                               const char* abs_dex_location,
                               std::string* error_msg);
  // Opens an oat file from an already opened File. Maps it PROT_READ, MAP_PRIVATE.
  static OatFile* OpenReadable(File* file, const std::string& location,
                               const char* abs_dex_location,
                               std::string* error_msg);

  ~OatFile();

  bool IsExecutable() const {
    return is_executable_;
  }

  bool IsPic() const;

  // Indicates whether the oat file was compiled with full debugging capability.
  bool IsDebuggable() const;

  ElfFile* GetElfFile() const {
    CHECK_NE(reinterpret_cast<uintptr_t>(elf_file_.get()), reinterpret_cast<uintptr_t>(nullptr))
        << "Cannot get an elf file from " << GetLocation();
    return elf_file_.get();
  }

  const std::string& GetLocation() const {
    return location_;
  }

  const OatHeader& GetOatHeader() const;

  class OatMethod FINAL {
   public:
    void LinkMethod(ArtMethod* method) const;

    uint32_t GetCodeOffset() const {
      return code_offset_;
    }

    const void* GetQuickCode() const {
      return GetOatPointer<const void*>(code_offset_);
    }

    // Returns size of quick code.
    uint32_t GetQuickCodeSize() const;
    uint32_t GetQuickCodeSizeOffset() const;

    // Returns OatQuickMethodHeader for debugging. Most callers should
    // use more specific methods such as GetQuickCodeSize.
    const OatQuickMethodHeader* GetOatQuickMethodHeader() const;
    uint32_t GetOatQuickMethodHeaderOffset() const;

    size_t GetFrameSizeInBytes() const;
    uint32_t GetCoreSpillMask() const;
    uint32_t GetFpSpillMask() const;

    const uint8_t* GetMappingTable() const;
    uint32_t GetMappingTableOffset() const;
    uint32_t GetMappingTableOffsetOffset() const;

    const uint8_t* GetVmapTable() const;
    uint32_t GetVmapTableOffset() const;
    uint32_t GetVmapTableOffsetOffset() const;

    const uint8_t* GetGcMap() const;
    uint32_t GetGcMapOffset() const;
    uint32_t GetGcMapOffsetOffset() const;

    // Create an OatMethod with offsets relative to the given base address
    OatMethod(const uint8_t* base, const uint32_t code_offset)
        : begin_(base), code_offset_(code_offset) {
    }
    OatMethod(const OatMethod&) = default;
    ~OatMethod() {}

    OatMethod& operator=(const OatMethod&) = default;

    // A representation of an invalid OatMethod, used when an OatMethod or OatClass can't be found.
    // See ClassLinker::FindOatMethodFor.
    static const OatMethod Invalid() {
      return OatMethod(nullptr, -1);
    }

   private:
    template<class T>
    T GetOatPointer(uint32_t offset) const {
      if (offset == 0) {
        return nullptr;
      }
      return reinterpret_cast<T>(begin_ + offset);
    }

    const uint8_t* begin_;
    uint32_t code_offset_;

    friend class OatClass;
  };

  class OatClass FINAL {
   public:
    mirror::Class::Status GetStatus() const {
      return status_;
    }

    OatClassType GetType() const {
      return type_;
    }

    // Get the OatMethod entry based on its index into the class
    // defintion. Direct methods come first, followed by virtual
    // methods. Note that runtime created methods such as miranda
    // methods are not included.
    const OatMethod GetOatMethod(uint32_t method_index) const;

    // Return a pointer to the OatMethodOffsets for the requested
    // method_index, or null if none is present. Note that most
    // callers should use GetOatMethod.
    const OatMethodOffsets* GetOatMethodOffsets(uint32_t method_index) const;

    // Return the offset from the start of the OatFile to the
    // OatMethodOffsets for the requested method_index, or 0 if none
    // is present. Note that most callers should use GetOatMethod.
    uint32_t GetOatMethodOffsetsOffset(uint32_t method_index) const;

    // A representation of an invalid OatClass, used when an OatClass can't be found.
    // See ClassLinker::FindOatClass.
    static OatClass Invalid() {
      return OatClass(nullptr, mirror::Class::kStatusError, kOatClassNoneCompiled, 0, nullptr,
                      nullptr);
    }

   private:
    OatClass(const OatFile* oat_file,
             mirror::Class::Status status,
             OatClassType type,
             uint32_t bitmap_size,
             const uint32_t* bitmap_pointer,
             const OatMethodOffsets* methods_pointer);

    const OatFile* const oat_file_;

    const mirror::Class::Status status_;

    const OatClassType type_;

    const uint32_t* const bitmap_;

    const OatMethodOffsets* const methods_pointer_;

    friend class art::OatDexFile;
  };
  const OatDexFile* GetOatDexFile(const char* dex_location,
                                  const uint32_t* const dex_location_checksum,
                                  bool exception_if_not_found = true) const
      LOCKS_EXCLUDED(secondary_lookup_lock_);

  const std::vector<const OatDexFile*>& GetOatDexFiles() const {
    return oat_dex_files_storage_;
  }

  size_t Size() const {
    return End() - Begin();
  }

  size_t BssSize() const {
    return BssEnd() - BssBegin();
  }

  const uint8_t* Begin() const;
  const uint8_t* End() const;

  const uint8_t* BssBegin() const;
  const uint8_t* BssEnd() const;

  // Returns the absolute dex location for the encoded relative dex location.
  //
  // If not null, abs_dex_location is used to resolve the absolute dex
  // location of relative dex locations encoded in the oat file.
  // For example, given absolute location "/data/app/foo/base.apk", encoded
  // dex locations "base.apk", "base.apk:classes2.dex", etc. would be resolved
  // to "/data/app/foo/base.apk", "/data/app/foo/base.apk:classes2.dex", etc.
  // Relative encoded dex locations that don't match the given abs_dex_location
  // are left unchanged.
  static std::string ResolveRelativeEncodedDexLocation(
      const char* abs_dex_location, const std::string& rel_dex_location);

  // Create a dependency list (dex locations and checksums) for the given dex files.
  static std::string EncodeDexFileDependencies(const std::vector<const DexFile*>& dex_files);

  // Check the given dependency list against their dex files - thus the name "Static," this does
  // not check the class-loader environment, only whether there have been file updates.
  static bool CheckStaticDexFileDependencies(const char* dex_dependencies, std::string* msg);

  // Get the dex locations of a dependency list. Note: this is *not* cleaned for synthetic
  // locations of multidex files.
  static bool GetDexLocationsFromDependencies(const char* dex_dependencies,
                                              std::vector<std::string>* locations);

 private:
  static void CheckLocation(const std::string& location);

  static OatFile* OpenDlopen(const std::string& elf_filename,
                             const std::string& location,
                             uint8_t* requested_base,
                             const char* abs_dex_location,
                             std::string* error_msg);

  static OatFile* OpenElfFile(File* file,
                              const std::string& location,
                              uint8_t* requested_base,
                              uint8_t* oat_file_begin,  // Override base if not null
                              bool writable,
                              bool executable,
                              const char* abs_dex_location,
                              std::string* error_msg);

  explicit OatFile(const std::string& filename, bool executable);
  bool Dlopen(const std::string& elf_filename, uint8_t* requested_base,
              const char* abs_dex_location, std::string* error_msg);
  bool ElfFileOpen(File* file, uint8_t* requested_base,
                   uint8_t* oat_file_begin,  // Override where the file is loaded to if not null
                   bool writable, bool executable,
                   const char* abs_dex_location,
                   std::string* error_msg);

  bool Setup(const char* abs_dex_location, std::string* error_msg);

  // The oat file name.
  //
  // The image will embed this to link its associated oat file.
  const std::string location_;

  // Pointer to OatHeader.
  const uint8_t* begin_;

  // Pointer to end of oat region for bounds checking.
  const uint8_t* end_;

  // Pointer to the .bss section, if present, otherwise null.
  const uint8_t* bss_begin_;

  // Pointer to the end of the .bss section, if present, otherwise null.
  const uint8_t* bss_end_;

  // Was this oat_file loaded executable?
  const bool is_executable_;

  // Backing memory map for oat file during when opened by ElfWriter during initial compilation.
  std::unique_ptr<MemMap> mem_map_;

  // Backing memory map for oat file during cross compilation.
  std::unique_ptr<ElfFile> elf_file_;

  // dlopen handle during runtime.
  void* dlopen_handle_;

  // Dummy memory map objects corresponding to the regions mapped by dlopen.
  std::vector<std::unique_ptr<MemMap>> dlopen_mmaps_;

  // Owning storage for the OatDexFile objects.
  std::vector<const OatDexFile*> oat_dex_files_storage_;

  // NOTE: We use a StringPiece as the key type to avoid a memory allocation on every
  // lookup with a const char* key. The StringPiece doesn't own its backing storage,
  // therefore we're using the OatDexFile::dex_file_location_ as the backing storage
  // for keys in oat_dex_files_ and the string_cache_ entries for the backing storage
  // of keys in secondary_oat_dex_files_ and oat_dex_files_by_canonical_location_.
  typedef AllocationTrackingSafeMap<StringPiece, const OatDexFile*, kAllocatorTagOatFile> Table;

  // Map each location and canonical location (if different) retrieved from the
  // oat file to its OatDexFile. This map doesn't change after it's constructed in Setup()
  // and therefore doesn't need any locking and provides the cheapest dex file lookup
  // for GetOatDexFile() for a very frequent use case. Never contains a null value.
  Table oat_dex_files_;

  // Lock guarding all members needed for secondary lookup in GetOatDexFile().
  mutable Mutex secondary_lookup_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // If the primary oat_dex_files_ lookup fails, use a secondary map. This map stores
  // the results of all previous secondary lookups, whether successful (non-null) or
  // failed (null). If it doesn't contain an entry we need to calculate the canonical
  // location and use oat_dex_files_by_canonical_location_.
  mutable Table secondary_oat_dex_files_ GUARDED_BY(secondary_lookup_lock_);

  // Cache of strings. Contains the backing storage for keys in the secondary_oat_dex_files_
  // and the lazily initialized oat_dex_files_by_canonical_location_.
  // NOTE: We're keeping references to contained strings in form of StringPiece and adding
  // new strings to the end. The adding of a new element must not touch any previously stored
  // elements. std::list<> and std::deque<> satisfy this requirement, std::vector<> doesn't.
  mutable std::list<std::string> string_cache_ GUARDED_BY(secondary_lookup_lock_);

  friend class OatClass;
  friend class art::OatDexFile;
  friend class OatDumper;  // For GetBase and GetLimit
  DISALLOW_COPY_AND_ASSIGN(OatFile);
};

// OatDexFile should be an inner class of OatFile. Unfortunately, C++ doesn't
// support forward declarations of inner classes, and we want to
// forward-declare OatDexFile so that we can store an opaque pointer to an
// OatDexFile in DexFile.
class OatDexFile FINAL {
 public:
  // Opens the DexFile referred to by this OatDexFile from within the containing OatFile.
  std::unique_ptr<const DexFile> OpenDexFile(std::string* error_msg) const;

  const OatFile* GetOatFile() const {
    return oat_file_;
  }

  // Returns the size of the DexFile refered to by this OatDexFile.
  size_t FileSize() const;

  // Returns original path of DexFile that was the source of this OatDexFile.
  const std::string& GetDexFileLocation() const {
    return dex_file_location_;
  }

  // Returns the canonical location of DexFile that was the source of this OatDexFile.
  const std::string& GetCanonicalDexFileLocation() const {
    return canonical_dex_file_location_;
  }

  // Returns checksum of original DexFile that was the source of this OatDexFile;
  uint32_t GetDexFileLocationChecksum() const {
    return dex_file_location_checksum_;
  }

  // Returns the OatClass for the class specified by the given DexFile class_def_index.
  OatFile::OatClass GetOatClass(uint16_t class_def_index) const;

  // Returns the offset to the OatClass information. Most callers should use GetOatClass.
  uint32_t GetOatClassOffset(uint16_t class_def_index) const;

  ~OatDexFile();

 private:
  OatDexFile(const OatFile* oat_file,
             const std::string& dex_file_location,
             const std::string& canonical_dex_file_location,
             uint32_t dex_file_checksum,
             const uint8_t* dex_file_pointer,
             const uint32_t* oat_class_offsets_pointer);

  const OatFile* const oat_file_;
  const std::string dex_file_location_;
  const std::string canonical_dex_file_location_;
  const uint32_t dex_file_location_checksum_;
  const uint8_t* const dex_file_pointer_;
  const uint32_t* const oat_class_offsets_pointer_;

  friend class OatFile;
  DISALLOW_COPY_AND_ASSIGN(OatDexFile);
};

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_H_
