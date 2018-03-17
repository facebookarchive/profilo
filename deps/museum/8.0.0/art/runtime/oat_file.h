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

#include "base/array_ref.h"
#include "base/mutex.h"
#include "base/stringpiece.h"
#include "compiler_filter.h"
#include "dex_file.h"
#include "mirror/class.h"
#include "oat.h"
#include "os.h"
#include "type_lookup_table.h"
#include "utf.h"
#include "utils.h"

namespace art {

class BitVector;
class ElfFile;
template <class MirrorType> class GcRoot;
class MemMap;
class OatMethodOffsets;
class OatHeader;
class OatDexFile;
class VdexFile;

namespace gc {
namespace collector {
class DummyOatFile;
}  // namespace collector
}  // namespace gc

// Runtime representation of the OAT file format which holds compiler output.
// The class opens an OAT file from storage and maps it to memory, typically with
// dlopen and provides access to its internal data structures (see OatWriter for
// for more details about the OAT format).
// In the process of loading OAT, the class also loads the associated VDEX file
// with the input DEX files (see VdexFile for details about the VDEX format).
// The raw DEX data are accessible transparently through the OatDexFile objects.

class OatFile {
 public:
  // Special classpath that skips shared library check.
  static constexpr const char* kSpecialSharedLibrary = "&";

  typedef art::OatDexFile OatDexFile;

  // Opens an oat file contained within the given elf file. This is always opened as
  // non-executable at the moment.
  static OatFile* OpenWithElfFile(ElfFile* elf_file,
                                  VdexFile* vdex_file,
                                  const std::string& location,
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
                       bool low_4gb,
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

  virtual ~OatFile();

  bool IsExecutable() const {
    return is_executable_;
  }

  bool IsPic() const;

  // Indicates whether the oat file was compiled with full debugging capability.
  bool IsDebuggable() const;

  CompilerFilter::Filter GetCompilerFilter() const;

  const std::string& GetLocation() const {
    return location_;
  }

  const OatHeader& GetOatHeader() const;

  class OatMethod FINAL {
   public:
    void LinkMethod(ArtMethod* method) const;

    uint32_t GetCodeOffset() const;

    const void* GetQuickCode() const;

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

    const uint8_t* GetVmapTable() const;
    uint32_t GetVmapTableOffset() const;
    uint32_t GetVmapTableOffsetOffset() const;

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
    // See FindOatClass().
    static OatClass Invalid() {
      return OatClass(/* oat_file */ nullptr,
                      mirror::Class::kStatusErrorUnresolved,
                      kOatClassNoneCompiled,
                      /* bitmap_size */ 0,
                      /* bitmap_pointer */ nullptr,
                      /* methods_pointer */ nullptr);
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

  // Get the OatDexFile for the given dex_location within this oat file.
  // If dex_location_checksum is non-null, the OatDexFile will only be
  // returned if it has a matching checksum.
  // If error_msg is non-null and no OatDexFile is returned, error_msg will
  // be updated with a description of why no OatDexFile was returned.
  const OatDexFile* GetOatDexFile(const char* dex_location,
                                  const uint32_t* const dex_location_checksum,
                                  /*out*/std::string* error_msg = nullptr) const
      REQUIRES(!secondary_lookup_lock_);

  const std::vector<const OatDexFile*>& GetOatDexFiles() const {
    return oat_dex_files_storage_;
  }

  size_t Size() const {
    return End() - Begin();
  }

  bool Contains(const void* p) const {
    return p >= Begin() && p < End();
  }

  size_t BssSize() const {
    return BssEnd() - BssBegin();
  }

  size_t BssRootsOffset() const {
    return bss_roots_ - BssBegin();
  }

  size_t DexSize() const {
    return DexEnd() - DexBegin();
  }

  const uint8_t* Begin() const;
  const uint8_t* End() const;

  const uint8_t* BssBegin() const;
  const uint8_t* BssEnd() const;

  const uint8_t* DexBegin() const;
  const uint8_t* DexEnd() const;

  ArrayRef<GcRoot<mirror::Object>> GetBssGcRoots() const;

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
  // Removes dex file paths prefixed with base_dir to convert them back to relative paths.
  static std::string EncodeDexFileDependencies(const std::vector<const DexFile*>& dex_files,
                                               std::string& base_dir);

  // Finds the associated oat class for a dex_file and descriptor. Returns an invalid OatClass on
  // error and sets found to false.
  static OatClass FindOatClass(const DexFile& dex_file, uint16_t class_def_idx, bool* found);

  VdexFile* GetVdexFile() const {
    return vdex_.get();
  }

 protected:
  OatFile(const std::string& filename, bool executable);

 private:
  // The oat file name.
  //
  // The image will embed this to link its associated oat file.
  const std::string location_;

  // Pointer to the Vdex file with the Dex files for this Oat file.
  std::unique_ptr<VdexFile> vdex_;

  // Pointer to OatHeader.
  const uint8_t* begin_;

  // Pointer to end of oat region for bounds checking.
  const uint8_t* end_;

  // Pointer to the .bss section, if present, otherwise null.
  uint8_t* bss_begin_;

  // Pointer to the end of the .bss section, if present, otherwise null.
  uint8_t* bss_end_;

  // Pointer to the beginning of the GC roots in .bss section, if present, otherwise null.
  uint8_t* bss_roots_;

  // Was this oat_file loaded executable?
  const bool is_executable_;

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

  friend class gc::collector::DummyOatFile;  // For modifying begin_ and end_.
  friend class OatClass;
  friend class art::OatDexFile;
  friend class OatDumper;  // For GetBase and GetLimit
  friend class OatFileBase;
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

  // May return null if the OatDexFile only contains a type lookup table. This case only happens
  // for the compiler to speed up compilation.
  const OatFile* GetOatFile() const {
    // Avoid pulling in runtime.h in the header file.
    if (kIsDebugBuild && oat_file_ == nullptr) {
      AssertAotCompiler();
    }
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

  uint8_t* GetDexCacheArrays() const {
    return dex_cache_arrays_;
  }

  const uint8_t* GetLookupTableData() const {
    return lookup_table_data_;
  }

  const uint8_t* GetDexFilePointer() const {
    return dex_file_pointer_;
  }

  // Looks up a class definition by its class descriptor. Hash must be
  // ComputeModifiedUtf8Hash(descriptor).
  static const DexFile::ClassDef* FindClassDef(const DexFile& dex_file,
                                               const char* descriptor,
                                               size_t hash);

  TypeLookupTable* GetTypeLookupTable() const {
    return lookup_table_.get();
  }

  ~OatDexFile();

  // Create only with a type lookup table, used by the compiler to speed up compilation.
  explicit OatDexFile(std::unique_ptr<TypeLookupTable>&& lookup_table);

 private:
  OatDexFile(const OatFile* oat_file,
             const std::string& dex_file_location,
             const std::string& canonical_dex_file_location,
             uint32_t dex_file_checksum,
             const uint8_t* dex_file_pointer,
             const uint8_t* lookup_table_data,
             const uint32_t* oat_class_offsets_pointer,
             uint8_t* dex_cache_arrays);

  static void AssertAotCompiler();

  const OatFile* const oat_file_ = nullptr;
  const std::string dex_file_location_;
  const std::string canonical_dex_file_location_;
  const uint32_t dex_file_location_checksum_ = 0u;
  const uint8_t* const dex_file_pointer_ = nullptr;
  const uint8_t* lookup_table_data_ = nullptr;
  const uint32_t* const oat_class_offsets_pointer_ = 0u;
  uint8_t* const dex_cache_arrays_ = nullptr;
  mutable std::unique_ptr<TypeLookupTable> lookup_table_;

  friend class OatFile;
  friend class OatFileBase;
  DISALLOW_COPY_AND_ASSIGN(OatDexFile);
};

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_H_
