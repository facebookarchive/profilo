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

#ifndef ART_RUNTIME_DEX_TO_DEX_DECOMPILER_H_
#define ART_RUNTIME_DEX_TO_DEX_DECOMPILER_H_

#include "base/array_ref.h"
#include "dex_file.h"

namespace art {
namespace optimizer {

// "Decompile", that is unquicken, the code item provided, given the
// associated quickening data.
// TODO: code_item isn't really a const element, but changing it
// to non-const has too many repercussions on the code base. We make it
// consistent with DexToDexCompiler, but we should really change it to
// DexFile::CodeItem*.
bool ArtDecompileDEX(const DexFile::CodeItem& code_item,
                     const ArrayRef<const uint8_t>& quickened_data,
                     bool decompile_return_instruction);

}  // namespace optimizer
}  // namespace art

#endif  // ART_RUNTIME_DEX_TO_DEX_DECOMPILER_H_
