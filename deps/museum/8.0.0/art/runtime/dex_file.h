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

#ifndef ART_RUNTIME_DEX_FILE_H_
#define ART_RUNTIME_DEX_FILE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/value_object.h"
#include "dex_file_types.h"
#include "globals.h"
#include "invoke_type.h"
#include "jni.h"
#include "modifiers.h"
#include "utf.h"

namespace art {

class MemMap;
class OatDexFile;
class Signature;
class StringPiece;
class ZipArchive;

class DexFile {
 public:
  // First Dex format version supporting default methods.
  static const uint32_t kDefaultMethodsVersion = 37;
  // First Dex format version enforcing class definition ordering rules.
  static const uint32_t kClassDefinitionOrderEnforcedVersion = 37;

  static const uint8_t kDexMagic[];
  static constexpr size_t kNumDexVersions = 3;
  static constexpr size_t kDexVersionLen = 4;
  static const uint8_t kDexMagicVersions[kNumDexVersions][kDexVersionLen];

  static constexpr size_t kSha1DigestSize = 20;
  static constexpr uint32_t kDexEndianConstant = 0x12345678;

  // name of the DexFile entry within a zip archive
  static const char* kClassesDex;

  // The value of an invalid index.
  static const uint32_t kDexNoIndex = 0xFFFFFFFF;

  // The value of an invalid index.
  static const uint16_t kDexNoIndex16 = 0xFFFF;

  // The separator character in MultiDex locations.
  static constexpr char kMultiDexSeparator = ':';

  // A string version of the previous. This is a define so that we can merge string literals in the
  // preprocessor.
  #define kMultiDexSeparatorString ":"

  // Raw header_item.
  struct Header {
    uint8_t magic_[8];
    uint32_t checksum_;  // See also location_checksum_
    uint8_t signature_[kSha1DigestSize];
    uint32_t file_size_;  // size of entire file
    uint32_t header_size_;  // offset to start of next section
    uint32_t endian_tag_;
    uint32_t link_size_;  // unused
    uint32_t link_off_;  // unused
    uint32_t map_off_;  // unused
    uint32_t string_ids_size_;  // number of StringIds
    uint32_t string_ids_off_;  // file offset of StringIds array
    uint32_t type_ids_size_;  // number of TypeIds, we don't support more than 65535
    uint32_t type_ids_off_;  // file offset of TypeIds array
    uint32_t proto_ids_size_;  // number of ProtoIds, we don't support more than 65535
    uint32_t proto_ids_off_;  // file offset of ProtoIds array
    uint32_t field_ids_size_;  // number of FieldIds
    uint32_t field_ids_off_;  // file offset of FieldIds array
    uint32_t method_ids_size_;  // number of MethodIds
    uint32_t method_ids_off_;  // file offset of MethodIds array
    uint32_t class_defs_size_;  // number of ClassDefs
    uint32_t class_defs_off_;  // file offset of ClassDef array
    uint32_t data_size_;  // size of data section
    uint32_t data_off_;  // file offset of data section

    // Decode the dex magic version
    uint32_t GetVersion() const;

   private:
    DISALLOW_COPY_AND_ASSIGN(Header);
  };

  // Map item type codes.
  enum MapItemType : uint16_t {  // private
    kDexTypeHeaderItem               = 0x0000,
    kDexTypeStringIdItem             = 0x0001,
    kDexTypeTypeIdItem               = 0x0002,
    kDexTypeProtoIdItem              = 0x0003,
    kDexTypeFieldIdItem              = 0x0004,
    kDexTypeMethodIdItem             = 0x0005,
    kDexTypeClassDefItem             = 0x0006,
    kDexTypeCallSiteIdItem           = 0x0007,
    kDexTypeMethodHandleItem         = 0x0008,
    kDexTypeMapList                  = 0x1000,
    kDexTypeTypeList                 = 0x1001,
    kDexTypeAnnotationSetRefList     = 0x1002,
    kDexTypeAnnotationSetItem        = 0x1003,
    kDexTypeClassDataItem            = 0x2000,
    kDexTypeCodeItem                 = 0x2001,
    kDexTypeStringDataItem           = 0x2002,
    kDexTypeDebugInfoItem            = 0x2003,
    kDexTypeAnnotationItem           = 0x2004,
    kDexTypeEncodedArrayItem         = 0x2005,
    kDexTypeAnnotationsDirectoryItem = 0x2006,
  };

  struct MapItem {
    uint16_t type_;
    uint16_t unused_;
    uint32_t size_;
    uint32_t offset_;

   private:
    DISALLOW_COPY_AND_ASSIGN(MapItem);
  };

  struct MapList {
    uint32_t size_;
    MapItem list_[1];

   private:
    DISALLOW_COPY_AND_ASSIGN(MapList);
  };

  // Raw string_id_item.
  struct StringId {
    uint32_t string_data_off_;  // offset in bytes from the base address

   private:
    DISALLOW_COPY_AND_ASSIGN(StringId);
  };

  // Raw type_id_item.
  struct TypeId {
    dex::StringIndex descriptor_idx_;  // index into string_ids

   private:
    DISALLOW_COPY_AND_ASSIGN(TypeId);
  };

  // Raw field_id_item.
  struct FieldId {
    dex::TypeIndex class_idx_;   // index into type_ids_ array for defining class
    dex::TypeIndex type_idx_;    // index into type_ids_ array for field type
    dex::StringIndex name_idx_;  // index into string_ids_ array for field name

   private:
    DISALLOW_COPY_AND_ASSIGN(FieldId);
  };

  // Raw proto_id_item.
  struct ProtoId {
    dex::StringIndex shorty_idx_;     // index into string_ids array for shorty descriptor
    dex::TypeIndex return_type_idx_;  // index into type_ids array for return type
    uint16_t pad_;                    // padding = 0
    uint32_t parameters_off_;         // file offset to type_list for parameter types

   private:
    DISALLOW_COPY_AND_ASSIGN(ProtoId);
  };

  // Raw method_id_item.
  struct MethodId {
    dex::TypeIndex class_idx_;   // index into type_ids_ array for defining class
    uint16_t proto_idx_;         // index into proto_ids_ array for method prototype
    dex::StringIndex name_idx_;  // index into string_ids_ array for method name

   private:
    DISALLOW_COPY_AND_ASSIGN(MethodId);
  };

  // Raw class_def_item.
  struct ClassDef {
    dex::TypeIndex class_idx_;  // index into type_ids_ array for this class
    uint16_t pad1_;  // padding = 0
    uint32_t access_flags_;
    dex::TypeIndex superclass_idx_;  // index into type_ids_ array for superclass
    uint16_t pad2_;  // padding = 0
    uint32_t interfaces_off_;  // file offset to TypeList
    dex::StringIndex source_file_idx_;  // index into string_ids_ for source file name
    uint32_t annotations_off_;  // file offset to annotations_directory_item
    uint32_t class_data_off_;  // file offset to class_data_item
    uint32_t static_values_off_;  // file offset to EncodedArray

    // Returns the valid access flags, that is, Java modifier bits relevant to the ClassDef type
    // (class or interface). These are all in the lower 16b and do not contain runtime flags.
    uint32_t GetJavaAccessFlags() const {
      // Make sure that none of our runtime-only flags are set.
      static_assert((kAccValidClassFlags & kAccJavaFlagsMask) == kAccValidClassFlags,
                    "Valid class flags not a subset of Java flags");
      static_assert((kAccValidInterfaceFlags & kAccJavaFlagsMask) == kAccValidInterfaceFlags,
                    "Valid interface flags not a subset of Java flags");

      if ((access_flags_ & kAccInterface) != 0) {
        // Interface.
        return access_flags_ & kAccValidInterfaceFlags;
      } else {
        // Class.
        return access_flags_ & kAccValidClassFlags;
      }
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ClassDef);
  };

  // Raw type_item.
  struct TypeItem {
    dex::TypeIndex type_idx_;  // index into type_ids section

   private:
    DISALLOW_COPY_AND_ASSIGN(TypeItem);
  };

  // Raw type_list.
  class TypeList {
   public:
    uint32_t Size() const {
      return size_;
    }

    const TypeItem& GetTypeItem(uint32_t idx) const {
      DCHECK_LT(idx, this->size_);
      return this->list_[idx];
    }

    // Size in bytes of the part of the list that is common.
    static constexpr size_t GetHeaderSize() {
      return 4U;
    }

    // Size in bytes of the whole type list including all the stored elements.
    static constexpr size_t GetListSize(size_t count) {
      return GetHeaderSize() + sizeof(TypeItem) * count;
    }

   private:
    uint32_t size_;  // size of the list, in entries
    TypeItem list_[1];  // elements of the list
    DISALLOW_COPY_AND_ASSIGN(TypeList);
  };

  // MethodHandle Types
  enum class MethodHandleType : uint16_t {  // private
    kStaticPut         = 0x0000,  // a setter for a given static field.
    kStaticGet         = 0x0001,  // a getter for a given static field.
    kInstancePut       = 0x0002,  // a setter for a given instance field.
    kInstanceGet       = 0x0003,  // a getter for a given instance field.
    kInvokeStatic      = 0x0004,  // an invoker for a given static method.
    kInvokeInstance    = 0x0005,  // invoke_instance : an invoker for a given instance method. This
                                  // can be any non-static method on any class (or interface) except
                                  // for “<init>”.
    kInvokeConstructor = 0x0006,  // an invoker for a given constructor.
    kLast = kInvokeConstructor
  };

  // raw method_handle_item
  struct MethodHandleItem {
    uint16_t method_handle_type_;
    uint16_t reserved1_;            // Reserved for future use.
    uint16_t field_or_method_idx_;  // Field index for accessors, method index otherwise.
    uint16_t reserved2_;            // Reserved for future use.
   private:
    DISALLOW_COPY_AND_ASSIGN(MethodHandleItem);
  };

  // raw call_site_id_item
  struct CallSiteIdItem {
    uint32_t data_off_;  // Offset into data section pointing to encoded array items.
   private:
    DISALLOW_COPY_AND_ASSIGN(CallSiteIdItem);
  };

  // Raw code_item.
  struct CodeItem {
    uint16_t registers_size_;            // the number of registers used by this code
                                         //   (locals + parameters)
    uint16_t ins_size_;                  // the number of words of incoming arguments to the method
                                         //   that this code is for
    uint16_t outs_size_;                 // the number of words of outgoing argument space required
                                         //   by this code for method invocation
    uint16_t tries_size_;                // the number of try_items for this instance. If non-zero,
                                         //   then these appear as the tries array just after the
                                         //   insns in this instance.
    uint32_t debug_info_off_;            // file offset to debug info stream
    uint32_t insns_size_in_code_units_;  // size of the insns array, in 2 byte code units
    uint16_t insns_[1];                  // actual array of bytecode.

   private:
    DISALLOW_COPY_AND_ASSIGN(CodeItem);
  };

  // Raw try_item.
  struct TryItem {
    uint32_t start_addr_;
    uint16_t insn_count_;
    uint16_t handler_off_;

   private:
    DISALLOW_COPY_AND_ASSIGN(TryItem);
  };

  // Annotation constants.
  enum {
    kDexVisibilityBuild         = 0x00,     /* annotation visibility */
    kDexVisibilityRuntime       = 0x01,
    kDexVisibilitySystem        = 0x02,

    kDexAnnotationByte          = 0x00,
    kDexAnnotationShort         = 0x02,
    kDexAnnotationChar          = 0x03,
    kDexAnnotationInt           = 0x04,
    kDexAnnotationLong          = 0x06,
    kDexAnnotationFloat         = 0x10,
    kDexAnnotationDouble        = 0x11,
    kDexAnnotationMethodType    = 0x15,
    kDexAnnotationMethodHandle  = 0x16,
    kDexAnnotationString        = 0x17,
    kDexAnnotationType          = 0x18,
    kDexAnnotationField         = 0x19,
    kDexAnnotationMethod        = 0x1a,
    kDexAnnotationEnum          = 0x1b,
    kDexAnnotationArray         = 0x1c,
    kDexAnnotationAnnotation    = 0x1d,
    kDexAnnotationNull          = 0x1e,
    kDexAnnotationBoolean       = 0x1f,

    kDexAnnotationValueTypeMask = 0x1f,     /* low 5 bits */
    kDexAnnotationValueArgShift = 5,
  };

  struct AnnotationsDirectoryItem {
    uint32_t class_annotations_off_;
    uint32_t fields_size_;
    uint32_t methods_size_;
    uint32_t parameters_size_;

   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);
  };

  struct FieldAnnotationsItem {
    uint32_t field_idx_;
    uint32_t annotations_off_;

   private:
    DISALLOW_COPY_AND_ASSIGN(FieldAnnotationsItem);
  };

  struct MethodAnnotationsItem {
    uint32_t method_idx_;
    uint32_t annotations_off_;

   private:
    DISALLOW_COPY_AND_ASSIGN(MethodAnnotationsItem);
  };

  struct ParameterAnnotationsItem {
    uint32_t method_idx_;
    uint32_t annotations_off_;

   private:
    DISALLOW_COPY_AND_ASSIGN(ParameterAnnotationsItem);
  };

  struct AnnotationSetRefItem {
    uint32_t annotations_off_;

   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationSetRefItem);
  };

  struct AnnotationSetRefList {
    uint32_t size_;
    AnnotationSetRefItem list_[1];

   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationSetRefList);
  };

  struct AnnotationSetItem {
    uint32_t size_;
    uint32_t entries_[1];

   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
  };

  struct AnnotationItem {
    uint8_t visibility_;
    uint8_t annotation_[1];

   private:
    DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
  };

  enum AnnotationResultStyle {  // private
    kAllObjects,
    kPrimitivesOrObjects,
    kAllRaw
  };

  struct AnnotationValue;

  // Returns the checksums of a file for comparison with GetLocationChecksum().
  // For .dex files, this is the single header checksum.
  // For zip files, this is the zip entry CRC32 checksum for classes.dex and
  // each additional multidex entry classes2.dex, classes3.dex, etc.
  // Return true if the checksums could be found, false otherwise.
  static bool GetMultiDexChecksums(const char* filename,
                                   std::vector<uint32_t>* checksums,
                                   std::string* error_msg);

  // Check whether a location denotes a multidex dex file. This is a very simple check: returns
  // whether the string contains the separator character.
  static bool IsMultiDexLocation(const char* location);

  // Opens .dex file, backed by existing memory
  static std::unique_ptr<const DexFile> Open(const uint8_t* base,
                                             size_t size,
                                             const std::string& location,
                                             uint32_t location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg);

  // Opens .dex file that has been memory-mapped by the caller.
  static std::unique_ptr<const DexFile> Open(const std::string& location,
                                             uint32_t location_checkum,
                                             std::unique_ptr<MemMap> mem_map,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg);

  // Opens all .dex files found in the file, guessing the container format based on file extension.
  static bool Open(const char* filename,
                   const std::string& location,
                   bool verify_checksum,
                   std::string* error_msg,
                   std::vector<std::unique_ptr<const DexFile>>* dex_files);

  // Open a single dex file from an fd.
  static std::unique_ptr<const DexFile> OpenDex(int fd,
                                                const std::string& location,
                                                bool verify_checksum,
                                                std::string* error_msg);

  // Opens dex files from within a .jar, .zip, or .apk file
  static bool OpenZip(int fd,
                      const std::string& location,
                      bool verify_checksum,
                      std::string* error_msg,
                      std::vector<std::unique_ptr<const DexFile>>* dex_files);

  // Closes a .dex file.
  virtual ~DexFile();

  const std::string& GetLocation() const {
    return location_;
  }

  // For normal dex files, location and base location coincide. If a dex file is part of a multidex
  // archive, the base location is the name of the originating jar/apk, stripped of any internal
  // classes*.dex path.
  static std::string GetBaseLocation(const char* location) {
    const char* pos = strrchr(location, kMultiDexSeparator);
    if (pos == nullptr) {
      return location;
    } else {
      return std::string(location, pos - location);
    }
  }

  static std::string GetBaseLocation(const std::string& location) {
    return GetBaseLocation(location.c_str());
  }

  // Returns the ':classes*.dex' part of the dex location. Returns an empty
  // string if there is no multidex suffix for the given location.
  // The kMultiDexSeparator is included in the returned suffix.
  static std::string GetMultiDexSuffix(const std::string& location) {
    size_t pos = location.rfind(kMultiDexSeparator);
    if (pos == std::string::npos) {
      return "";
    } else {
      return location.substr(pos);
    }
  }

  std::string GetBaseLocation() const {
    return GetBaseLocation(location_);
  }

  // For DexFiles directly from .dex files, this is the checksum from the DexFile::Header.
  // For DexFiles opened from a zip files, this will be the ZipEntry CRC32 of classes.dex.
  uint32_t GetLocationChecksum() const {
    return location_checksum_;
  }

  const Header& GetHeader() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return *header_;
  }

  // Decode the dex magic version
  uint32_t GetVersion() const {
    return GetHeader().GetVersion();
  }

  // Returns true if the byte string points to the magic value.
  static bool IsMagicValid(const uint8_t* magic);

  // Returns true if the byte string after the magic is the correct value.
  static bool IsVersionValid(const uint8_t* magic);

  // Returns the number of string identifiers in the .dex file.
  size_t NumStringIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->string_ids_size_;
  }

  // Returns the StringId at the specified index.
  const StringId& GetStringId(dex::StringIndex idx) const {
    DCHECK_LT(idx.index_, NumStringIds()) << GetLocation();
    return string_ids_[idx.index_];
  }

  dex::StringIndex GetIndexForStringId(const StringId& string_id) const {
    CHECK_GE(&string_id, string_ids_) << GetLocation();
    CHECK_LT(&string_id, string_ids_ + header_->string_ids_size_) << GetLocation();
    return dex::StringIndex(&string_id - string_ids_);
  }

  int32_t GetStringLength(const StringId& string_id) const;

  // Returns a pointer to the UTF-8 string data referred to by the given string_id as well as the
  // length of the string when decoded as a UTF-16 string. Note the UTF-16 length is not the same
  // as the string length of the string data.
  const char* GetStringDataAndUtf16Length(const StringId& string_id, uint32_t* utf16_length) const;

  const char* GetStringData(const StringId& string_id) const;

  // Index version of GetStringDataAndUtf16Length.
  const char* StringDataAndUtf16LengthByIdx(dex::StringIndex idx, uint32_t* utf16_length) const;

  const char* StringDataByIdx(dex::StringIndex idx) const;

  // Looks up a string id for a given modified utf8 string.
  const StringId* FindStringId(const char* string) const;

  const TypeId* FindTypeId(const char* string) const;

  // Looks up a string id for a given utf16 string.
  const StringId* FindStringId(const uint16_t* string, size_t length) const;

  // Returns the number of type identifiers in the .dex file.
  uint32_t NumTypeIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->type_ids_size_;
  }

  bool IsTypeIndexValid(dex::TypeIndex idx) const {
    return idx.IsValid() && idx.index_ < NumTypeIds();
  }

  // Returns the TypeId at the specified index.
  const TypeId& GetTypeId(dex::TypeIndex idx) const {
    DCHECK_LT(idx.index_, NumTypeIds()) << GetLocation();
    return type_ids_[idx.index_];
  }

  dex::TypeIndex GetIndexForTypeId(const TypeId& type_id) const {
    CHECK_GE(&type_id, type_ids_) << GetLocation();
    CHECK_LT(&type_id, type_ids_ + header_->type_ids_size_) << GetLocation();
    size_t result = &type_id - type_ids_;
    DCHECK_LT(result, 65536U) << GetLocation();
    return dex::TypeIndex(static_cast<uint16_t>(result));
  }

  // Get the descriptor string associated with a given type index.
  const char* StringByTypeIdx(dex::TypeIndex idx, uint32_t* unicode_length) const;

  const char* StringByTypeIdx(dex::TypeIndex idx) const;

  // Returns the type descriptor string of a type id.
  const char* GetTypeDescriptor(const TypeId& type_id) const;

  // Looks up a type for the given string index
  const TypeId* FindTypeId(dex::StringIndex string_idx) const;

  // Returns the number of field identifiers in the .dex file.
  size_t NumFieldIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->field_ids_size_;
  }

  // Returns the FieldId at the specified index.
  const FieldId& GetFieldId(uint32_t idx) const {
    DCHECK_LT(idx, NumFieldIds()) << GetLocation();
    return field_ids_[idx];
  }

  uint32_t GetIndexForFieldId(const FieldId& field_id) const {
    CHECK_GE(&field_id, field_ids_) << GetLocation();
    CHECK_LT(&field_id, field_ids_ + header_->field_ids_size_) << GetLocation();
    return &field_id - field_ids_;
  }

  // Looks up a field by its declaring class, name and type
  const FieldId* FindFieldId(const DexFile::TypeId& declaring_klass,
                             const DexFile::StringId& name,
                             const DexFile::TypeId& type) const;

  uint32_t FindCodeItemOffset(const DexFile::ClassDef& class_def,
                              uint32_t dex_method_idx) const;

  // Returns the declaring class descriptor string of a field id.
  const char* GetFieldDeclaringClassDescriptor(const FieldId& field_id) const {
    const DexFile::TypeId& type_id = GetTypeId(field_id.class_idx_);
    return GetTypeDescriptor(type_id);
  }

  // Returns the class descriptor string of a field id.
  const char* GetFieldTypeDescriptor(const FieldId& field_id) const;

  // Returns the name of a field id.
  const char* GetFieldName(const FieldId& field_id) const;

  // Returns the number of method identifiers in the .dex file.
  size_t NumMethodIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->method_ids_size_;
  }

  // Returns the MethodId at the specified index.
  const MethodId& GetMethodId(uint32_t idx) const {
    DCHECK_LT(idx, NumMethodIds()) << GetLocation();
    return method_ids_[idx];
  }

  uint32_t GetIndexForMethodId(const MethodId& method_id) const {
    CHECK_GE(&method_id, method_ids_) << GetLocation();
    CHECK_LT(&method_id, method_ids_ + header_->method_ids_size_) << GetLocation();
    return &method_id - method_ids_;
  }

  // Looks up a method by its declaring class, name and proto_id
  const MethodId* FindMethodId(const DexFile::TypeId& declaring_klass,
                               const DexFile::StringId& name,
                               const DexFile::ProtoId& signature) const;

  // Returns the declaring class descriptor string of a method id.
  const char* GetMethodDeclaringClassDescriptor(const MethodId& method_id) const;

  // Returns the prototype of a method id.
  const ProtoId& GetMethodPrototype(const MethodId& method_id) const {
    return GetProtoId(method_id.proto_idx_);
  }

  // Returns a representation of the signature of a method id.
  const Signature GetMethodSignature(const MethodId& method_id) const;

  // Returns a representation of the signature of a proto id.
  const Signature GetProtoSignature(const ProtoId& proto_id) const;

  // Returns the name of a method id.
  const char* GetMethodName(const MethodId& method_id) const;

  // Returns the shorty of a method by its index.
  const char* GetMethodShorty(uint32_t idx) const;

  // Returns the shorty of a method id.
  const char* GetMethodShorty(const MethodId& method_id) const;
  const char* GetMethodShorty(const MethodId& method_id, uint32_t* length) const;

  // Returns the number of class definitions in the .dex file.
  uint32_t NumClassDefs() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->class_defs_size_;
  }

  // Returns the ClassDef at the specified index.
  const ClassDef& GetClassDef(uint16_t idx) const {
    DCHECK_LT(idx, NumClassDefs()) << GetLocation();
    return class_defs_[idx];
  }

  uint16_t GetIndexForClassDef(const ClassDef& class_def) const {
    CHECK_GE(&class_def, class_defs_) << GetLocation();
    CHECK_LT(&class_def, class_defs_ + header_->class_defs_size_) << GetLocation();
    return &class_def - class_defs_;
  }

  // Returns the class descriptor string of a class definition.
  const char* GetClassDescriptor(const ClassDef& class_def) const;

  // Looks up a class definition by its type index.
  const ClassDef* FindClassDef(dex::TypeIndex type_idx) const;

  const TypeList* GetInterfacesList(const ClassDef& class_def) const {
    if (class_def.interfaces_off_ == 0) {
        return nullptr;
    } else {
      const uint8_t* addr = begin_ + class_def.interfaces_off_;
      return reinterpret_cast<const TypeList*>(addr);
    }
  }

  uint32_t NumMethodHandles() const {
    return num_method_handles_;
  }

  const MethodHandleItem& GetMethodHandle(uint32_t idx) const {
    CHECK_LT(idx, NumMethodHandles());
    return method_handles_[idx];
  }

  uint32_t NumCallSiteIds() const {
    return num_call_site_ids_;
  }

  const CallSiteIdItem& GetCallSiteId(uint32_t idx) const {
    CHECK_LT(idx, NumCallSiteIds());
    return call_site_ids_[idx];
  }

  // Returns a pointer to the raw memory mapped class_data_item
  const uint8_t* GetClassData(const ClassDef& class_def) const {
    if (class_def.class_data_off_ == 0) {
      return nullptr;
    } else {
      return begin_ + class_def.class_data_off_;
    }
  }

  //
  const CodeItem* GetCodeItem(const uint32_t code_off) const {
    DCHECK_LT(code_off, size_) << "Code item offset larger then maximum allowed offset";
    if (code_off == 0) {
      return nullptr;  // native or abstract method
    } else {
      const uint8_t* addr = begin_ + code_off;
      return reinterpret_cast<const CodeItem*>(addr);
    }
  }

  const char* GetReturnTypeDescriptor(const ProtoId& proto_id) const;

  // Returns the number of prototype identifiers in the .dex file.
  size_t NumProtoIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->proto_ids_size_;
  }

  // Returns the ProtoId at the specified index.
  const ProtoId& GetProtoId(uint16_t idx) const {
    DCHECK_LT(idx, NumProtoIds()) << GetLocation();
    return proto_ids_[idx];
  }

  uint16_t GetIndexForProtoId(const ProtoId& proto_id) const {
    CHECK_GE(&proto_id, proto_ids_) << GetLocation();
    CHECK_LT(&proto_id, proto_ids_ + header_->proto_ids_size_) << GetLocation();
    return &proto_id - proto_ids_;
  }

  // Looks up a proto id for a given return type and signature type list
  const ProtoId* FindProtoId(dex::TypeIndex return_type_idx,
                             const dex::TypeIndex* signature_type_idxs,
                             uint32_t signature_length) const;
  const ProtoId* FindProtoId(dex::TypeIndex return_type_idx,
                             const std::vector<dex::TypeIndex>& signature_type_idxs) const {
    return FindProtoId(return_type_idx, &signature_type_idxs[0], signature_type_idxs.size());
  }

  // Given a signature place the type ids into the given vector, returns true on success
  bool CreateTypeList(const StringPiece& signature,
                      dex::TypeIndex* return_type_idx,
                      std::vector<dex::TypeIndex>* param_type_idxs) const;

  // Create a Signature from the given string signature or return Signature::NoSignature if not
  // possible.
  const Signature CreateSignature(const StringPiece& signature) const;

  // Returns the short form method descriptor for the given prototype.
  const char* GetShorty(uint32_t proto_idx) const;

  const TypeList* GetProtoParameters(const ProtoId& proto_id) const {
    if (proto_id.parameters_off_ == 0) {
      return nullptr;
    } else {
      const uint8_t* addr = begin_ + proto_id.parameters_off_;
      return reinterpret_cast<const TypeList*>(addr);
    }
  }

  const uint8_t* GetEncodedStaticFieldValuesArray(const ClassDef& class_def) const {
    if (class_def.static_values_off_ == 0) {
      return 0;
    } else {
      return begin_ + class_def.static_values_off_;
    }
  }

  const uint8_t* GetCallSiteEncodedValuesArray(const CallSiteIdItem& call_site_id) const {
    return begin_ + call_site_id.data_off_;
  }

  static const TryItem* GetTryItems(const CodeItem& code_item, uint32_t offset);

  // Get the base of the encoded data for the given DexCode.
  static const uint8_t* GetCatchHandlerData(const CodeItem& code_item, uint32_t offset) {
    const uint8_t* handler_data =
        reinterpret_cast<const uint8_t*>(GetTryItems(code_item, code_item.tries_size_));
    return handler_data + offset;
  }

  // Find which try region is associated with the given address (ie dex pc). Returns -1 if none.
  static int32_t FindTryItem(const CodeItem &code_item, uint32_t address);

  // Find the handler offset associated with the given address (ie dex pc). Returns -1 if none.
  static int32_t FindCatchHandlerOffset(const CodeItem &code_item, uint32_t address);

  // Get the pointer to the start of the debugging data
  const uint8_t* GetDebugInfoStream(const CodeItem* code_item) const {
    // Check that the offset is in bounds.
    // Note that although the specification says that 0 should be used if there
    // is no debug information, some applications incorrectly use 0xFFFFFFFF.
    if (code_item->debug_info_off_ == 0 || code_item->debug_info_off_ >= size_) {
      return nullptr;
    } else {
      return begin_ + code_item->debug_info_off_;
    }
  }

  struct PositionInfo {
    PositionInfo()
        : address_(0),
          line_(0),
          source_file_(nullptr),
          prologue_end_(false),
          epilogue_begin_(false) {
    }

    uint32_t address_;  // In 16-bit code units.
    uint32_t line_;  // Source code line number starting at 1.
    const char* source_file_;  // nullptr if the file from ClassDef still applies.
    bool prologue_end_;
    bool epilogue_begin_;
  };

  // Callback for "new position table entry".
  // Returning true causes the decoder to stop early.
  typedef bool (*DexDebugNewPositionCb)(void* context, const PositionInfo& entry);

  struct LocalInfo {
    LocalInfo()
        : name_(nullptr),
          descriptor_(nullptr),
          signature_(nullptr),
          start_address_(0),
          end_address_(0),
          reg_(0),
          is_live_(false) {
    }

    const char* name_;  // E.g., list.  It can be nullptr if unknown.
    const char* descriptor_;  // E.g., Ljava/util/LinkedList;
    const char* signature_;  // E.g., java.util.LinkedList<java.lang.Integer>
    uint32_t start_address_;  // PC location where the local is first defined.
    uint32_t end_address_;  // PC location where the local is no longer defined.
    uint16_t reg_;  // Dex register which stores the values.
    bool is_live_;  // Is the local defined and live.
  };

  // Callback for "new locals table entry".
  typedef void (*DexDebugNewLocalCb)(void* context, const LocalInfo& entry);

  static bool LineNumForPcCb(void* context, const PositionInfo& entry);

  const AnnotationsDirectoryItem* GetAnnotationsDirectory(const ClassDef& class_def) const {
    if (class_def.annotations_off_ == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const AnnotationsDirectoryItem*>(begin_ + class_def.annotations_off_);
    }
  }

  const AnnotationSetItem* GetClassAnnotationSet(const AnnotationsDirectoryItem* anno_dir) const {
    if (anno_dir->class_annotations_off_ == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const AnnotationSetItem*>(begin_ + anno_dir->class_annotations_off_);
    }
  }

  const FieldAnnotationsItem* GetFieldAnnotations(const AnnotationsDirectoryItem* anno_dir) const {
    if (anno_dir->fields_size_ == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const FieldAnnotationsItem*>(&anno_dir[1]);
    }
  }

  const MethodAnnotationsItem* GetMethodAnnotations(const AnnotationsDirectoryItem* anno_dir)
      const {
    if (anno_dir->methods_size_ == 0) {
      return nullptr;
    } else {
      // Skip past the header and field annotations.
      const uint8_t* addr = reinterpret_cast<const uint8_t*>(&anno_dir[1]);
      addr += anno_dir->fields_size_ * sizeof(FieldAnnotationsItem);
      return reinterpret_cast<const MethodAnnotationsItem*>(addr);
    }
  }

  const ParameterAnnotationsItem* GetParameterAnnotations(const AnnotationsDirectoryItem* anno_dir)
      const {
    if (anno_dir->parameters_size_ == 0) {
      return nullptr;
    } else {
      // Skip past the header, field annotations, and method annotations.
      const uint8_t* addr = reinterpret_cast<const uint8_t*>(&anno_dir[1]);
      addr += anno_dir->fields_size_ * sizeof(FieldAnnotationsItem);
      addr += anno_dir->methods_size_ * sizeof(MethodAnnotationsItem);
      return reinterpret_cast<const ParameterAnnotationsItem*>(addr);
    }
  }

  const AnnotationSetItem* GetFieldAnnotationSetItem(const FieldAnnotationsItem& anno_item) const {
    uint32_t offset = anno_item.annotations_off_;
    if (offset == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const AnnotationSetItem*>(begin_ + offset);
    }
  }

  const AnnotationSetItem* GetMethodAnnotationSetItem(const MethodAnnotationsItem& anno_item)
      const {
    uint32_t offset = anno_item.annotations_off_;
    if (offset == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const AnnotationSetItem*>(begin_ + offset);
    }
  }

  const AnnotationSetRefList* GetParameterAnnotationSetRefList(
      const ParameterAnnotationsItem* anno_item) const {
    uint32_t offset = anno_item->annotations_off_;
    if (offset == 0) {
      return nullptr;
    }
    return reinterpret_cast<const AnnotationSetRefList*>(begin_ + offset);
  }

  const AnnotationItem* GetAnnotationItem(const AnnotationSetItem* set_item, uint32_t index) const {
    DCHECK_LE(index, set_item->size_);
    uint32_t offset = set_item->entries_[index];
    if (offset == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const AnnotationItem*>(begin_ + offset);
    }
  }

  const AnnotationSetItem* GetSetRefItemItem(const AnnotationSetRefItem* anno_item) const {
    uint32_t offset = anno_item->annotations_off_;
    if (offset == 0) {
      return nullptr;
    }
    return reinterpret_cast<const AnnotationSetItem*>(begin_ + offset);
  }

  // Debug info opcodes and constants
  enum {
    DBG_END_SEQUENCE         = 0x00,
    DBG_ADVANCE_PC           = 0x01,
    DBG_ADVANCE_LINE         = 0x02,
    DBG_START_LOCAL          = 0x03,
    DBG_START_LOCAL_EXTENDED = 0x04,
    DBG_END_LOCAL            = 0x05,
    DBG_RESTART_LOCAL        = 0x06,
    DBG_SET_PROLOGUE_END     = 0x07,
    DBG_SET_EPILOGUE_BEGIN   = 0x08,
    DBG_SET_FILE             = 0x09,
    DBG_FIRST_SPECIAL        = 0x0a,
    DBG_LINE_BASE            = -4,
    DBG_LINE_RANGE           = 15,
  };

  struct LineNumFromPcContext {
    LineNumFromPcContext(uint32_t address, uint32_t line_num)
        : address_(address), line_num_(line_num) {}
    uint32_t address_;
    uint32_t line_num_;
   private:
    DISALLOW_COPY_AND_ASSIGN(LineNumFromPcContext);
  };

  // Returns false if there is no debugging information or if it cannot be decoded.
  bool DecodeDebugLocalInfo(const CodeItem* code_item, bool is_static, uint32_t method_idx,
                            DexDebugNewLocalCb local_cb, void* context) const;

  // Returns false if there is no debugging information or if it cannot be decoded.
  bool DecodeDebugPositionInfo(const CodeItem* code_item, DexDebugNewPositionCb position_cb,
                               void* context) const;

  const char* GetSourceFile(const ClassDef& class_def) const {
    if (!class_def.source_file_idx_.IsValid()) {
      return nullptr;
    } else {
      return StringDataByIdx(class_def.source_file_idx_);
    }
  }

  int GetPermissions() const;

  bool IsReadOnly() const;

  bool EnableWrite() const;

  bool DisableWrite() const;

  const uint8_t* Begin() const {
    return begin_;
  }

  size_t Size() const {
    return size_;
  }

  // Return the name of the index-th classes.dex in a multidex zip file. This is classes.dex for
  // index == 0, and classes{index + 1}.dex else.
  static std::string GetMultiDexClassesDexName(size_t index);

  // Return the (possibly synthetic) dex location for a multidex entry. This is dex_location for
  // index == 0, and dex_location + multi-dex-separator + GetMultiDexClassesDexName(index) else.
  static std::string GetMultiDexLocation(size_t index, const char* dex_location);

  // Returns the canonical form of the given dex location.
  //
  // There are different flavors of "dex locations" as follows:
  // the file name of a dex file:
  //     The actual file path that the dex file has on disk.
  // dex_location:
  //     This acts as a key for the class linker to know which dex file to load.
  //     It may correspond to either an old odex file or a particular dex file
  //     inside an oat file. In the first case it will also match the file name
  //     of the dex file. In the second case (oat) it will include the file name
  //     and possibly some multidex annotation to uniquely identify it.
  // canonical_dex_location:
  //     the dex_location where it's file name part has been made canonical.
  static std::string GetDexCanonicalLocation(const char* dex_location);

  const OatDexFile* GetOatDexFile() const {
    return oat_dex_file_;
  }

  // Used by oat writer.
  void SetOatDexFile(OatDexFile* oat_dex_file) const {
    oat_dex_file_ = oat_dex_file;
  }

  // Utility methods for reading integral values from a buffer.
  static int32_t ReadSignedInt(const uint8_t* ptr, int zwidth);
  static uint32_t ReadUnsignedInt(const uint8_t* ptr, int zwidth, bool fill_on_right);
  static int64_t ReadSignedLong(const uint8_t* ptr, int zwidth);
  static uint64_t ReadUnsignedLong(const uint8_t* ptr, int zwidth, bool fill_on_right);

  // Recalculates the checksum of the dex file. Does not use the current value in the header.
  uint32_t CalculateChecksum() const;

  // Returns a human-readable form of the method at an index.
  std::string PrettyMethod(uint32_t method_idx, bool with_signature = true) const;
  // Returns a human-readable form of the field at an index.
  std::string PrettyField(uint32_t field_idx, bool with_type = true) const;
  // Returns a human-readable form of the type at an index.
  std::string PrettyType(dex::TypeIndex type_idx) const;

 private:
  static std::unique_ptr<const DexFile> OpenFile(int fd,
                                                 const std::string& location,
                                                 bool verify,
                                                 bool verify_checksum,
                                                 std::string* error_msg);

  enum class ZipOpenErrorCode {  // private
    kNoError,
    kEntryNotFound,
    kExtractToMemoryError,
    kDexFileError,
    kMakeReadOnlyError,
    kVerifyError
  };

  // Open all classesXXX.dex files from a zip archive.
  static bool OpenAllDexFilesFromZip(const ZipArchive& zip_archive,
                                     const std::string& location,
                                     bool verify_checksum,
                                     std::string* error_msg,
                                     std::vector<std::unique_ptr<const DexFile>>* dex_files);

  // Opens .dex file from the entry_name in a zip archive. error_code is undefined when non-null
  // return.
  static std::unique_ptr<const DexFile> OpenOneDexFileFromZip(const ZipArchive& zip_archive,
                                                              const char* entry_name,
                                                              const std::string& location,
                                                              bool verify_checksum,
                                                              std::string* error_msg,
                                                              ZipOpenErrorCode* error_code);

  enum class VerifyResult {  // private
    kVerifyNotAttempted,
    kVerifySucceeded,
    kVerifyFailed
  };

  static std::unique_ptr<DexFile> OpenCommon(const uint8_t* base,
                                             size_t size,
                                             const std::string& location,
                                             uint32_t location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg,
                                             VerifyResult* verify_result = nullptr);


  // Opens a .dex file at the given address, optionally backed by a MemMap
  static std::unique_ptr<const DexFile> OpenMemory(const uint8_t* dex_file,
                                                   size_t size,
                                                   const std::string& location,
                                                   uint32_t location_checksum,
                                                   std::unique_ptr<MemMap> mem_map,
                                                   const OatDexFile* oat_dex_file,
                                                   std::string* error_msg);

  DexFile(const uint8_t* base,
          size_t size,
          const std::string& location,
          uint32_t location_checksum,
          const OatDexFile* oat_dex_file);

  // Top-level initializer that calls other Init methods.
  bool Init(std::string* error_msg);

  // Returns true if the header magic and version numbers are of the expected values.
  bool CheckMagicAndVersion(std::string* error_msg) const;

  // Initialize section info for sections only found in map. Returns true on success.
  void InitializeSectionsFromMapList();

  // The base address of the memory mapping.
  const uint8_t* const begin_;

  // The size of the underlying memory allocation in bytes.
  const size_t size_;

  // Typically the dex file name when available, alternatively some identifying string.
  //
  // The ClassLinker will use this to match DexFiles the boot class
  // path to DexCache::GetLocation when loading from an image.
  const std::string location_;

  const uint32_t location_checksum_;

  // Manages the underlying memory allocation.
  std::unique_ptr<MemMap> mem_map_;

  // Points to the header section.
  const Header* const header_;

  // Points to the base of the string identifier list.
  const StringId* const string_ids_;

  // Points to the base of the type identifier list.
  const TypeId* const type_ids_;

  // Points to the base of the field identifier list.
  const FieldId* const field_ids_;

  // Points to the base of the method identifier list.
  const MethodId* const method_ids_;

  // Points to the base of the prototype identifier list.
  const ProtoId* const proto_ids_;

  // Points to the base of the class definition list.
  const ClassDef* const class_defs_;

  // Points to the base of the method handles list.
  const MethodHandleItem* method_handles_;

  // Number of elements in the method handles list.
  size_t num_method_handles_;

  // Points to the base of the call sites id list.
  const CallSiteIdItem* call_site_ids_;

  // Number of elements in the call sites list.
  size_t num_call_site_ids_;

  // If this dex file was loaded from an oat file, oat_dex_file_ contains a
  // pointer to the OatDexFile it was loaded from. Otherwise oat_dex_file_ is
  // null.
  mutable const OatDexFile* oat_dex_file_;

  friend class DexFileVerifierTest;
  friend class OatWriter;
  ART_FRIEND_TEST(ClassLinkerTest, RegisterDexFileName);  // for constructor
};

struct DexFileReference {
  DexFileReference(const DexFile* file, uint32_t idx) : dex_file(file), index(idx) { }
  const DexFile* dex_file;
  uint32_t index;
};

std::ostream& operator<<(std::ostream& os, const DexFile& dex_file);

// Iterate over a dex file's ProtoId's paramters
class DexFileParameterIterator {
 public:
  DexFileParameterIterator(const DexFile& dex_file, const DexFile::ProtoId& proto_id)
      : dex_file_(dex_file), size_(0), pos_(0) {
    type_list_ = dex_file_.GetProtoParameters(proto_id);
    if (type_list_ != nullptr) {
      size_ = type_list_->Size();
    }
  }
  bool HasNext() const { return pos_ < size_; }
  size_t Size() const { return size_; }
  void Next() { ++pos_; }
  dex::TypeIndex GetTypeIdx() {
    return type_list_->GetTypeItem(pos_).type_idx_;
  }
  const char* GetDescriptor() {
    return dex_file_.StringByTypeIdx(dex::TypeIndex(GetTypeIdx()));
  }
 private:
  const DexFile& dex_file_;
  const DexFile::TypeList* type_list_;
  uint32_t size_;
  uint32_t pos_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexFileParameterIterator);
};

// Abstract the signature of a method.
class Signature : public ValueObject {
 public:
  std::string ToString() const;

  static Signature NoSignature() {
    return Signature();
  }

  bool IsVoid() const;
  uint32_t GetNumberOfParameters() const;

  bool operator==(const Signature& rhs) const;
  bool operator!=(const Signature& rhs) const {
    return !(*this == rhs);
  }

  bool operator==(const StringPiece& rhs) const;

 private:
  Signature(const DexFile* dex, const DexFile::ProtoId& proto) : dex_file_(dex), proto_id_(&proto) {
  }

  Signature() : dex_file_(nullptr), proto_id_(nullptr) {
  }

  friend class DexFile;

  const DexFile* const dex_file_;
  const DexFile::ProtoId* const proto_id_;
};
std::ostream& operator<<(std::ostream& os, const Signature& sig);

// Iterate and decode class_data_item
class ClassDataItemIterator {
 public:
  ClassDataItemIterator(const DexFile& dex_file, const uint8_t* raw_class_data_item)
      : dex_file_(dex_file), pos_(0), ptr_pos_(raw_class_data_item), last_idx_(0) {
    ReadClassDataHeader();
    if (EndOfInstanceFieldsPos() > 0) {
      ReadClassDataField();
    } else if (EndOfVirtualMethodsPos() > 0) {
      ReadClassDataMethod();
    }
  }
  uint32_t NumStaticFields() const {
    return header_.static_fields_size_;
  }
  uint32_t NumInstanceFields() const {
    return header_.instance_fields_size_;
  }
  uint32_t NumDirectMethods() const {
    return header_.direct_methods_size_;
  }
  uint32_t NumVirtualMethods() const {
    return header_.virtual_methods_size_;
  }
  bool IsAtMethod() const {
    return pos_ >= EndOfInstanceFieldsPos();
  }
  bool HasNextStaticField() const {
    return pos_ < EndOfStaticFieldsPos();
  }
  bool HasNextInstanceField() const {
    return pos_ >= EndOfStaticFieldsPos() && pos_ < EndOfInstanceFieldsPos();
  }
  bool HasNextDirectMethod() const {
    return pos_ >= EndOfInstanceFieldsPos() && pos_ < EndOfDirectMethodsPos();
  }
  bool HasNextVirtualMethod() const {
    return pos_ >= EndOfDirectMethodsPos() && pos_ < EndOfVirtualMethodsPos();
  }
  bool HasNext() const {
    return pos_ < EndOfVirtualMethodsPos();
  }
  inline void Next() {
    pos_++;
    if (pos_ < EndOfStaticFieldsPos()) {
      last_idx_ = GetMemberIndex();
      ReadClassDataField();
    } else if (pos_ == EndOfStaticFieldsPos() && NumInstanceFields() > 0) {
      last_idx_ = 0;  // transition to next array, reset last index
      ReadClassDataField();
    } else if (pos_ < EndOfInstanceFieldsPos()) {
      last_idx_ = GetMemberIndex();
      ReadClassDataField();
    } else if (pos_ == EndOfInstanceFieldsPos() && NumDirectMethods() > 0) {
      last_idx_ = 0;  // transition to next array, reset last index
      ReadClassDataMethod();
    } else if (pos_ < EndOfDirectMethodsPos()) {
      last_idx_ = GetMemberIndex();
      ReadClassDataMethod();
    } else if (pos_ == EndOfDirectMethodsPos() && NumVirtualMethods() > 0) {
      last_idx_ = 0;  // transition to next array, reset last index
      ReadClassDataMethod();
    } else if (pos_ < EndOfVirtualMethodsPos()) {
      last_idx_ = GetMemberIndex();
      ReadClassDataMethod();
    } else {
      DCHECK(!HasNext());
    }
  }
  uint32_t GetMemberIndex() const {
    if (pos_ < EndOfInstanceFieldsPos()) {
      return last_idx_ + field_.field_idx_delta_;
    } else {
      DCHECK_LT(pos_, EndOfVirtualMethodsPos());
      return last_idx_ + method_.method_idx_delta_;
    }
  }
  uint32_t GetRawMemberAccessFlags() const {
    if (pos_ < EndOfInstanceFieldsPos()) {
      return field_.access_flags_;
    } else {
      DCHECK_LT(pos_, EndOfVirtualMethodsPos());
      return method_.access_flags_;
    }
  }
  uint32_t GetFieldAccessFlags() const {
    return GetRawMemberAccessFlags() & kAccValidFieldFlags;
  }
  uint32_t GetMethodAccessFlags() const {
    return GetRawMemberAccessFlags() & kAccValidMethodFlags;
  }
  bool MemberIsNative() const {
    return GetRawMemberAccessFlags() & kAccNative;
  }
  bool MemberIsFinal() const {
    return GetRawMemberAccessFlags() & kAccFinal;
  }
  InvokeType GetMethodInvokeType(const DexFile::ClassDef& class_def) const {
    if (HasNextDirectMethod()) {
      if ((GetRawMemberAccessFlags() & kAccStatic) != 0) {
        return kStatic;
      } else {
        return kDirect;
      }
    } else {
      DCHECK_EQ(GetRawMemberAccessFlags() & kAccStatic, 0U);
      if ((class_def.access_flags_ & kAccInterface) != 0) {
        return kInterface;
      } else if ((GetRawMemberAccessFlags() & kAccConstructor) != 0) {
        return kSuper;
      } else {
        return kVirtual;
      }
    }
  }
  const DexFile::CodeItem* GetMethodCodeItem() const {
    return dex_file_.GetCodeItem(method_.code_off_);
  }
  uint32_t GetMethodCodeItemOffset() const {
    return method_.code_off_;
  }
  const uint8_t* DataPointer() const {
    return ptr_pos_;
  }
  const uint8_t* EndDataPointer() const {
    CHECK(!HasNext());
    return ptr_pos_;
  }

 private:
  // A dex file's class_data_item is leb128 encoded, this structure holds a decoded form of the
  // header for a class_data_item
  struct ClassDataHeader {
    uint32_t static_fields_size_;  // the number of static fields
    uint32_t instance_fields_size_;  // the number of instance fields
    uint32_t direct_methods_size_;  // the number of direct methods
    uint32_t virtual_methods_size_;  // the number of virtual methods
  } header_;

  // Read and decode header from a class_data_item stream into header
  void ReadClassDataHeader();

  uint32_t EndOfStaticFieldsPos() const {
    return header_.static_fields_size_;
  }
  uint32_t EndOfInstanceFieldsPos() const {
    return EndOfStaticFieldsPos() + header_.instance_fields_size_;
  }
  uint32_t EndOfDirectMethodsPos() const {
    return EndOfInstanceFieldsPos() + header_.direct_methods_size_;
  }
  uint32_t EndOfVirtualMethodsPos() const {
    return EndOfDirectMethodsPos() + header_.virtual_methods_size_;
  }

  // A decoded version of the field of a class_data_item
  struct ClassDataField {
    uint32_t field_idx_delta_;  // delta of index into the field_ids array for FieldId
    uint32_t access_flags_;  // access flags for the field
    ClassDataField() :  field_idx_delta_(0), access_flags_(0) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ClassDataField);
  };
  ClassDataField field_;

  // Read and decode a field from a class_data_item stream into field
  void ReadClassDataField();

  // A decoded version of the method of a class_data_item
  struct ClassDataMethod {
    uint32_t method_idx_delta_;  // delta of index into the method_ids array for MethodId
    uint32_t access_flags_;
    uint32_t code_off_;
    ClassDataMethod() : method_idx_delta_(0), access_flags_(0), code_off_(0) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ClassDataMethod);
  };
  ClassDataMethod method_;

  // Read and decode a method from a class_data_item stream into method
  void ReadClassDataMethod();

  const DexFile& dex_file_;
  size_t pos_;  // integral number of items passed
  const uint8_t* ptr_pos_;  // pointer into stream of class_data_item
  uint32_t last_idx_;  // last read field or method index to apply delta to
  DISALLOW_IMPLICIT_CONSTRUCTORS(ClassDataItemIterator);
};

class EncodedArrayValueIterator {
 public:
  EncodedArrayValueIterator(const DexFile& dex_file, const uint8_t* array_data);

  bool HasNext() const { return pos_ < array_size_; }

  void Next();

  enum ValueType {
    kByte         = 0x00,
    kShort        = 0x02,
    kChar         = 0x03,
    kInt          = 0x04,
    kLong         = 0x06,
    kFloat        = 0x10,
    kDouble       = 0x11,
    kMethodType   = 0x15,
    kMethodHandle = 0x16,
    kString       = 0x17,
    kType         = 0x18,
    kField        = 0x19,
    kMethod       = 0x1a,
    kEnum         = 0x1b,
    kArray        = 0x1c,
    kAnnotation   = 0x1d,
    kNull         = 0x1e,
    kBoolean      = 0x1f,
  };

  ValueType GetValueType() const { return type_; }
  const jvalue& GetJavaValue() const { return jval_; }

 protected:
  static constexpr uint8_t kEncodedValueTypeMask = 0x1f;  // 0b11111
  static constexpr uint8_t kEncodedValueArgShift = 5;

  const DexFile& dex_file_;
  size_t array_size_;  // Size of array.
  size_t pos_;  // Current position.
  const uint8_t* ptr_;  // Pointer into encoded data array.
  ValueType type_;  // Type of current encoded value.
  jvalue jval_;  // Value of current encoded value.

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EncodedArrayValueIterator);
};
std::ostream& operator<<(std::ostream& os, const EncodedArrayValueIterator::ValueType& code);

class EncodedStaticFieldValueIterator : public EncodedArrayValueIterator {
 public:
  EncodedStaticFieldValueIterator(const DexFile& dex_file,
                                  const DexFile::ClassDef& class_def)
      : EncodedArrayValueIterator(dex_file,
                                  dex_file.GetEncodedStaticFieldValuesArray(class_def))
  {}

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EncodedStaticFieldValueIterator);
};
std::ostream& operator<<(std::ostream& os, const EncodedStaticFieldValueIterator::ValueType& code);

class CallSiteArrayValueIterator : public EncodedArrayValueIterator {
 public:
  CallSiteArrayValueIterator(const DexFile& dex_file,
                             const DexFile::CallSiteIdItem& call_site_id)
      : EncodedArrayValueIterator(dex_file,
                                  dex_file.GetCallSiteEncodedValuesArray(call_site_id))
  {}

  uint32_t Size() const { return array_size_; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallSiteArrayValueIterator);
};
std::ostream& operator<<(std::ostream& os, const CallSiteArrayValueIterator::ValueType& code);

class CatchHandlerIterator {
  public:
    CatchHandlerIterator(const DexFile::CodeItem& code_item, uint32_t address);

    CatchHandlerIterator(const DexFile::CodeItem& code_item,
                         const DexFile::TryItem& try_item);

    explicit CatchHandlerIterator(const uint8_t* handler_data) {
      Init(handler_data);
    }

    dex::TypeIndex GetHandlerTypeIndex() const {
      return handler_.type_idx_;
    }
    uint32_t GetHandlerAddress() const {
      return handler_.address_;
    }
    void Next();
    bool HasNext() const {
      return remaining_count_ != -1 || catch_all_;
    }
    // End of this set of catch blocks, convenience method to locate next set of catch blocks
    const uint8_t* EndDataPointer() const {
      CHECK(!HasNext());
      return current_data_;
    }

  private:
    void Init(const DexFile::CodeItem& code_item, int32_t offset);
    void Init(const uint8_t* handler_data);

    struct CatchHandlerItem {
      dex::TypeIndex type_idx_;  // type index of the caught exception type
      uint32_t address_;  // handler address
    } handler_;
    const uint8_t* current_data_;  // the current handler in dex file.
    int32_t remaining_count_;   // number of handlers not read.
    bool catch_all_;            // is there a handler that will catch all exceptions in case
                                // that all typed handler does not match.
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_FILE_H_
