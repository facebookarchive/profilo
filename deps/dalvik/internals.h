/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;
typedef int8_t s1;
typedef int16_t s2;
typedef int32_t s4;
typedef int64_t s8;

struct Thread;
typedef struct Thread Thread;
struct ClassObject;
typedef struct ClassObject ClassObject;
struct Object;
typedef struct Object Object;
struct Method;
typedef struct Method Method;
struct DexFile;
typedef struct DexFile DexFile;
struct ArrayObject;
typedef struct ArrayObject ArrayObject;
struct DexHeader;
typedef struct DexHeader DexHeader;
struct DexOptHeader;
typedef struct DexOptHeader DexOptHeader;
struct DexStringId;
typedef struct DexStringId DexStringId;
struct DexTypeId;
typedef struct DexTypeId DexTypeId;
struct DexTypeItem;
typedef struct DexTypeItem DexTypeItem;
struct DexFieldId;
typedef struct DexFieldId DexFieldId;
struct DexMethodId;
typedef struct DexMethodId DexMethodId;
struct DexProtoId;
typedef struct DexProtoId DexProtoId;
struct DexProto;
typedef struct DexProto DexProto;
struct DexClassDef;
typedef struct DexClassDef DexClassDef;
struct DexLink;
typedef struct DexLink DexLink;
struct DexClassLookup;
typedef struct DexClassLookup DexClassLookup;
struct StringObject;
typedef struct StringObject StringObject;
struct InterpSaveState;
typedef struct InterpSaveState InterpSaveState;
struct DvmDex;
typedef struct DvmDex DvmDex;
struct Field;
typedef struct Field Field;

union JValue {
  u1      z;
  s1      b;
  u2      c;
  s2      s;
  s4      i;
  s8      j;
  float   f;
  double  d;
  Object* l;
};
typedef union JValue JValue;

typedef void (*DalvikBridgeFunc)(
  const uint32_t* args,
  JValue* pResult,
  const Method* method,
  Thread* self);

struct Object {
  ClassObject* clazz;
  uint32_t lock;
};

typedef enum ClassStatus {
    CLASS_ERROR         = -1,

    CLASS_NOTREADY      = 0,
    CLASS_IDX           = 1,    /* loaded, DEX idx in super or ifaces */
    CLASS_LOADED        = 2,    /* DEX idx values resolved */
    CLASS_RESOLVED      = 3,    /* part of linking */
    CLASS_VERIFYING     = 4,    /* in the process of being verified */
    CLASS_VERIFIED      = 5,    /* logically part of linking; done pre-init */
    CLASS_INITIALIZING  = 6,    /* class init in progress */
    CLASS_INITIALIZED   = 7,    /* ready to go */
} ClassStatus;

enum ThreadStatus {
  THREAD_UNDEFINED    = -1,

  THREAD_ZOMBIE       = 0,
  THREAD_RUNNING      = 1,
  THREAD_TIMED_WAIT   = 2,
  THREAD_MONITOR      = 3,
  THREAD_WAIT         = 4,
  THREAD_INITIALIZING = 5,
  THREAD_STARTING     = 6,
  THREAD_NATIVE       = 7,
  THREAD_VMWAIT       = 8,
  THREAD_SUSPENDED    = 9,
};

typedef enum PrimitiveType {
    PRIM_NOT        = 0,
    PRIM_VOID       = 1,
    PRIM_BOOLEAN    = 2,
    PRIM_BYTE       = 3,
    PRIM_SHORT      = 4,
    PRIM_CHAR       = 5,
    PRIM_INT        = 6,
    PRIM_LONG       = 7,
    PRIM_FLOAT      = 8,
    PRIM_DOUBLE     = 9,
} PrimitiveType;

struct DvmDex {
  DexFile* pDexFile;
  const DexHeader* pHeader;
  StringObject** pResStrings;
  ClassObject** pResClasses;
};

struct DexClassLookup {
  int size;
  int numEntries;
  struct {
    u4 classDescriptorHash;
    int classDescriptorOffset;
    int classDefOffset;
  } table[1];
};

struct DexFile {
  const DexOptHeader* pOptHeader;
  const DexHeader*    pHeader;
  const DexStringId*  pStringIds;
  const DexTypeId*    pTypeIds;
  const DexFieldId*   pFieldIds;
  const DexMethodId*  pMethodIds;
  const DexProtoId*   pProtoIds;
  const DexClassDef*  pClassDefs;
  const DexLink*      pLinkData;
  const DexClassLookup* pClassLookup;
  const void*         pRegisterMapPool;
  const u1*           baseAddr;
  int                 overhead;
};

struct __attribute__((packed)) DexFileHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t checksum;
  uint8_t signature[20];
  uint32_t file_size;
  uint32_t header_size;
  uint32_t endian_tag;
  uint32_t link_size;

  uint32_t link_off;
  uint32_t map_off;
  uint32_t string_ids_size;
  uint32_t string_ids_off;
  uint32_t type_ids_size;
  uint32_t type_ids_off;
  uint32_t proto_ids_size;
  uint32_t proto_ids_off;
  uint32_t field_ids_size;
  uint32_t field_ids_off;
  uint32_t method_ids_size;
  uint32_t method_ids_off;
  uint32_t class_defs_size;
  uint32_t class_defs_off;
  uint32_t data_size;
  uint32_t data_off;
};

struct DexClassDef {
  u4 classIdx;
  u4 accessFlags;
  u4 superclassIdx;
  u4 interfacesOff;
  u4 sourceFileIdx;
  u4 annotationsOff;
  u4 classDataOff;
  u4 staticValuesOff;
};

struct DexTypeId {
  u4 descriptorIdx;
};

struct DexStringId {
  u4 stringDataOff;
};

struct DexMethodId {
  u2 classIdx;
  u2 protoIdx;
  u4 nameIdx;
};

struct DexFieldId {
  u2 classIdx;
  u2 typeIdx;
  u4 nameIdx;
};

#ifdef __cplusplus
struct ClassObject : Object {
#else
struct ClassObject {
  Object parent;
#endif
  uint32_t instanceData[4];
  const char* descriptor;
  char* descriptorAlloc;
  uint32_t accessFlags;
  u4 serialNumber;
  DvmDex* pDvmDex;
  int status;
  ClassStatus verifyErrorClass;
  u4 initThreadId;
  size_t objectSize;
  ClassObject* elementClass;
  int arrayDim;
  PrimitiveType primitiveType;
  ClassObject* super;
  Object* classLoader;
};

struct DexProto {
  const DexFile* dexFile;
  uint32_t protoIdx;
};

struct Method {
  ClassObject* clazz;
  uint32_t accessFlags;
  uint16_t methodIndex;
  uint16_t registersSize;
  uint16_t outsSize;
  uint16_t insSize;
  const char* name;
  DexProto prototype;
  const char* shorty;
  const uint16_t* insns;
  int jniArgInfo;
  DalvikBridgeFunc nativeFunc;
  /* Unstable bits follow... */
};

struct Field {
  ClassObject* clazz;
  const char* name;
  const char* signature;
  u4 accessFlags;
};

#ifdef __cplusplus
struct StaticField : Field {
#else
struct StaticField {
  Field parent;
#endif
  JValue value;
};

#ifdef __cplusplus
struct InstField : Field {
#else
struct InstField {
  Field parent;
#endif
  int byteOffset;
};

#ifdef __cplusplus
struct StringObject : Object {
#else
struct StringObject {
  Object parent;
#endif
  u4 instanceData[1];
};

struct DexProtoId {
  u4 shortyIdx;
  u4 returnTypeIdx;
  u4 parametersOff;
};

struct DexTypeItem {
  u2 typeIdx;
};

struct DexTypeList {
  u4 size;
  DexTypeItem list[1];
};

struct InterpSaveState {
  const u2* pc;
  u4* curFrame;
};

struct Thread {
  InterpSaveState interpSave;
};

struct StackSaveArea {
  u4*         prevFrame;
  const u2*   savedPc;
  const Method* method;
};

// These magic numbers on Android versions spanning Gingerbread to 5.0
// and beyond.  Dalvik is in maintenance mode now that Art is under
// heavy development.

#define DEX_OPT_MAGIC   "dey\n"
#define DEX_OPT_MAGIC_VERS  "036\0"

/*
 * 160-bit SHA-1 digest.
 */
enum { kSHA1DigestLen = 20,
       kSHA1DigestOutputLen = kSHA1DigestLen*2 +1 };

typedef struct DexOptHeader {
  uint8_t  magic[8];           /* includes version number */

  uint32_t  dexOffset;          /* file offset of DEX header */
  uint32_t  dexLength;
  uint32_t  depsOffset;         /* offset of optimized DEX dependency table */
  uint32_t  depsLength;
  uint32_t  optOffset;          /* file offset of optimized data tables */
  uint32_t  optLength;

  uint32_t  flags;              /* some info flags */
  uint32_t  checksum;           /* adler32 checksum covering deps/opt */

  /* pad for 64-bit alignment if necessary */
} DexOptHeader;

typedef struct DexHeader {
  uint8_t  magic[8];           /* includes version number */
  uint32_t  checksum;           /* adler32 checksum */
  uint8_t  signature[kSHA1DigestLen]; /* SHA-1 hash */
  uint32_t  fileSize;           /* length of entire file */
  uint32_t  headerSize;         /* offset to start of next section */
  uint32_t  endianTag;
  uint32_t  linkSize;
  uint32_t  linkOff;
  uint32_t  mapOff;
  uint32_t  stringIdsSize;
  uint32_t  stringIdsOff;
  uint32_t  typeIdsSize;
  uint32_t  typeIdsOff;
  uint32_t  protoIdsSize;
  uint32_t  protoIdsOff;
  uint32_t  fieldIdsSize;
  uint32_t  fieldIdsOff;
  uint32_t  methodIdsSize;
  uint32_t  methodIdsOff;
  uint32_t  classDefsSize;
  uint32_t  classDefsOff;
  uint32_t  dataSize;
  uint32_t  dataOff;
} DexHeader;

typedef struct DexDep {
  uint32_t nameLength;
  char* name;
  uint8_t* digest;
} DexDep;

typedef struct DexDeps {
  uint32_t srcModTime;
  uint32_t srcChecksum;
  uint32_t dalvikBuild;
  uint32_t nrDeps;
  char deps[0];
} DexDeps;


struct SymbolSpec {
  const char* name[2];
  void* value;
  bool optional;
};

void* open_libdvm();
void* open_libart();
void* open_libc();

void ensure_symbols(
  void* lib,
  const struct SymbolSpec* ss,
  unsigned nr_ss);

#ifdef __cplusplus
} // extern C
#endif

