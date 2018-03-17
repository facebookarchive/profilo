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

#ifndef ART_RUNTIME_IMTABLE_INL_H_
#define ART_RUNTIME_IMTABLE_INL_H_

#include "imtable.h"

#include "art_method-inl.h"
#include "dex_file.h"
#include "utf.h"

namespace art {

static constexpr bool kImTableHashUseName = true;
static constexpr bool kImTableHashUseCoefficients = true;

// Magic configuration that minimizes some common runtime calls.
static constexpr uint32_t kImTableHashCoefficientClass = 427;
static constexpr uint32_t kImTableHashCoefficientName = 16;
static constexpr uint32_t kImTableHashCoefficientSignature = 14;

inline void ImTable::GetImtHashComponents(ArtMethod* method,
                                          uint32_t* class_hash,
                                          uint32_t* name_hash,
                                          uint32_t* signature_hash) {
  if (kImTableHashUseName) {
    if (method->IsProxyMethod()) {
      *class_hash = 0;
      *name_hash = 0;
      *signature_hash = 0;
      return;
    }

    const DexFile* dex_file = method->GetDexFile();
    const DexFile::MethodId& method_id = dex_file->GetMethodId(method->GetDexMethodIndex());

    // Class descriptor for the class component.
    *class_hash = ComputeModifiedUtf8Hash(dex_file->GetMethodDeclaringClassDescriptor(method_id));

    // Method name for the method component.
    *name_hash = ComputeModifiedUtf8Hash(dex_file->GetMethodName(method_id));

    const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);

    // Read the proto for the signature component.
    uint32_t tmp = ComputeModifiedUtf8Hash(
        dex_file->GetTypeDescriptor(dex_file->GetTypeId(proto_id.return_type_idx_)));

    // Mix in the argument types.
    // Note: we could consider just using the shorty. This would be faster, at the price of
    //       potential collisions.
    const DexFile::TypeList* param_types = dex_file->GetProtoParameters(proto_id);
    if (param_types != nullptr) {
      for (size_t i = 0; i != param_types->Size(); ++i) {
        const DexFile::TypeItem& type = param_types->GetTypeItem(i);
        tmp = 31 * tmp + ComputeModifiedUtf8Hash(
            dex_file->GetTypeDescriptor(dex_file->GetTypeId(type.type_idx_)));
      }
    }

    *signature_hash = tmp;
    return;
  } else {
    *class_hash = method->GetDexMethodIndex();
    *name_hash = 0;
    *signature_hash = 0;
    return;
  }
}

inline uint32_t ImTable::GetImtIndex(ArtMethod* method) {
  uint32_t class_hash, name_hash, signature_hash;
  GetImtHashComponents(method, &class_hash, &name_hash, &signature_hash);

  uint32_t mixed_hash;
  if (!kImTableHashUseCoefficients) {
    mixed_hash = class_hash + name_hash + signature_hash;
  } else {
    mixed_hash = kImTableHashCoefficientClass * class_hash +
                 kImTableHashCoefficientName * name_hash +
                 kImTableHashCoefficientSignature * signature_hash;
  }

  return mixed_hash % ImTable::kSize;
}

}  // namespace art

#endif  // ART_RUNTIME_IMTABLE_INL_H_

