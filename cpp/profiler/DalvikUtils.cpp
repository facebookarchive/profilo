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

#include "DalvikUtils.h"

namespace facebook {
namespace profilo {
namespace profiler {

const DexProtoId* getProtoId(const DexProto* pProto) {
  return &pProto->dexFile->pProtoIds[pProto->protoIdx];
}

const DexTypeList* dexGetProtoParameters(
    const DexFile* pDexFile,
    const DexProtoId* pProtoId) {
  if (pProtoId->parametersOff == 0) {
    return nullptr;
  }
  return (const DexTypeList*)(pDexFile->baseAddr + pProtoId->parametersOff);
}

u4 dexTypeListGetIdx(const DexTypeList* pList, u4 idx) {
  const DexTypeItem* pItem = &pList->list[idx];
  return pItem->typeIdx;
}

const char* dexStringById(const DexFile* pDexFile, u4 idx) {
  const DexStringId* pStringId = &pDexFile->pStringIds[idx];

  const u1* str = pDexFile->baseAddr + pStringId->stringDataOff;

  // Skip the uleb128 length.
  while (*(str++) > 0x7f)
    ; /* empty */

  return (const char*)str;
}

const char* dexStringByTypeIdx(const DexFile* pDexFile, u4 idx) {
  const DexTypeId* pTypeId = &pDexFile->pTypeIds[idx];
  return dexStringById(pDexFile, pTypeId->descriptorIdx);
}

int dexProtoCompare(const DexProto* pProto1, const DexProto* pProto2) {
  if (pProto1 == pProto2) {
    return 0;
  } else {
    const DexFile* dexFile1 = pProto1->dexFile;
    const DexProtoId* protoId1 = getProtoId(pProto1);

    const DexTypeList* typeList1 = dexGetProtoParameters(dexFile1, protoId1);
    int paramCount1 = (typeList1 == nullptr) ? 0 : typeList1->size;

    const DexFile* dexFile2 = pProto2->dexFile;
    const DexProtoId* protoId2 = getProtoId(pProto2);
    const DexTypeList* typeList2 = dexGetProtoParameters(dexFile2, protoId2);
    int paramCount2 = (typeList2 == nullptr) ? 0 : typeList2->size;

    if (protoId1 == protoId2) {
      return 0;
    }

    int result = strcmp(
        dexStringByTypeIdx(dexFile1, protoId1->returnTypeIdx),
        dexStringByTypeIdx(dexFile2, protoId2->returnTypeIdx));

    if (result != 0) {
      return result;
    }

    int minParam = (paramCount1 > paramCount2) ? paramCount2 : paramCount1;
    int i;

    for (i = 0; i < minParam; i++) {
      u4 idx1 = dexTypeListGetIdx(typeList1, i);
      u4 idx2 = dexTypeListGetIdx(typeList2, i);
      result = strcmp(
          dexStringByTypeIdx(dexFile1, idx1),
          dexStringByTypeIdx(dexFile2, idx2));

      if (result != 0) {
        return result;
      }
    }

    if (paramCount1 < paramCount2) {
      return -1;
    } else if (paramCount1 > paramCount2) {
      return 1;
    } else {
      return 0;
    }
  }
}

int compareMethodStr(DexFile* pDexFile, u4 methodIdx, const Method* method) {
  const DexMethodId* pMethodId = &pDexFile->pMethodIds[methodIdx];
  const char* str = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);
  int result = strcmp(str, method->clazz->descriptor);

  if (result == 0) {
    str = dexStringById(pDexFile, pMethodId->nameIdx);
    result = strcmp(str, method->name);
    if (result == 0) {
      DexProto proto;
      proto.dexFile = pDexFile;
      proto.protoIdx = pMethodId->protoIdx;
      result = dexProtoCompare(&proto, &method->prototype);
    }
  }

  return result;
}

u4 getMethodIdx(const Method* method) {
  ClassObject* cls = method->clazz;
  DvmDex* dvmDex = cls->pDvmDex;
  // Can be null for VM-generated classes: e.g. arrays and Proxy classes.
  if (dvmDex == nullptr) {
    return 0;
  }
  DexFile* pDexFile = dvmDex->pDexFile;
  u4 hi = pDexFile->pHeader->methodIdsSize - 1;
  u4 lo = 0;
  u4 cur;

  while (hi >= lo) {
    int cmp;
    cur = (lo + hi) / 2;

    cmp = compareMethodStr(pDexFile, cur, method);
    if (cmp < 0) {
      lo = cur + 1;
    } else if (cmp > 0) {
      hi = cur - 1;
    } else {
      break;
    }
  }
  return cur;
}

u4 getDexSignatureFromMethod(const Method* method) {
  DvmDex* dvmDex = method->clazz->pDvmDex;
  if (dvmDex == nullptr) {
    return 0;
  }
  return *((u4*)dvmDex->pHeader->signature);
}

int64_t dalvikGetMethodIdForSymbolication(const Method* method) {
  u8 methodId = (u8)getMethodIdx(method);
  u4 dexSignature = getDexSignatureFromMethod(method);

  return methodId << 32 | dexSignature;
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
