/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_DEX_FILE_LAYOUT_H_
#define ART_RUNTIME_DEX_FILE_LAYOUT_H_

#include <cstdint>
#include <iosfwd>

namespace art {

class DexFile;

enum class LayoutType : uint8_t {
  // Layout of things that are randomly used. These should be advised to random access.
  // Without layout, this is the default mode when loading a dex file.
  kLayoutTypeSometimesUsed,
  // Layout of things that are only used during startup, these can be madvised after launch.
  kLayoutTypeStartupOnly,
  // Layout of things that are hot (commonly accessed), these should be pinned or madvised will
  // need.
  kLayoutTypeHot,
  // Layout of things that are needed probably only once (class initializers). These can be
  // madvised during trim events.
  kLayoutTypeUsedOnce,
  // Layout of things that are thought to be unused. These things should be advised to random
  // access.
  kLayoutTypeUnused,
  // Unused value, just the number of elements in the enum.
  kLayoutTypeCount,
};
std::ostream& operator<<(std::ostream& os, const LayoutType& collector_type);

enum class MadviseState : uint8_t {
  // Madvise based on a file that was just loaded.
  kMadviseStateAtLoad,
  // Madvise based after launch is finished.
  kMadviseStateFinishedLaunch,
  // Trim by madvising code that is unlikely to be too important in the future.
  kMadviseStateFinishedTrim,
};
std::ostream& operator<<(std::ostream& os, const MadviseState& collector_type);

// A dex layout section such as code items or strings. Each section is composed of subsections
// that are layed out ajacently to each other such as (hot, unused, startup, etc...).
class DexLayoutSection {
 public:
  // A subsection is a a continuous range of dex file that is all part of the same layout hint.
  class Subsection {
   public:
    // Use uint32_t to handle 32/64 bit cross compilation.
    uint32_t offset_ = 0u;
    uint32_t size_ = 0u;

    void Madvise(const DexFile* dex_file, int advice) const;
  };

  Subsection parts_[static_cast<size_t>(LayoutType::kLayoutTypeCount)];
};

// A set of dex layout sections, currently there is only one section for code and one for strings.
class DexLayoutSections {
 public:
  enum class SectionType : uint8_t {
    kSectionTypeCode,
    kSectionTypeStrings,
    kSectionCount,
  };

  // Advise access about the dex file based on layout. The caller is expected to have already
  // madvised to MADV_RANDOM.
  void Madvise(const DexFile* dex_file, MadviseState state) const;

  DexLayoutSection sections_[static_cast<size_t>(SectionType::kSectionCount)];
};

std::ostream& operator<<(std::ostream& os, const DexLayoutSections::SectionType& collector_type);
std::ostream& operator<<(std::ostream& os, const DexLayoutSection& section);
std::ostream& operator<<(std::ostream& os, const DexLayoutSections& sections);

}  // namespace art

#endif  // ART_RUNTIME_DEX_FILE_LAYOUT_H_
