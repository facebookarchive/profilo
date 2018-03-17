/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_DEX2OAT_ENVIRONMENT_TEST_H_
#define ART_RUNTIME_DEX2OAT_ENVIRONMENT_TEST_H_

#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "common_runtime_test.h"
#include "compiler_callbacks.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "oat_file_assistant.h"
#include "os.h"
#include "runtime.h"
#include "utils.h"

namespace art {

// Test class that provides some helpers to set a test up for compilation using dex2oat.
class Dex2oatEnvironmentTest : public CommonRuntimeTest {
 public:
  virtual void SetUp() OVERRIDE {
    CommonRuntimeTest::SetUp();

    // Create a scratch directory to work from.
    scratch_dir_ = android_data_ + "/Dex2oatEnvironmentTest";
    ASSERT_EQ(0, mkdir(scratch_dir_.c_str(), 0700));

    // Create a subdirectory in scratch for odex files.
    odex_oat_dir_ = scratch_dir_ + "/oat";
    ASSERT_EQ(0, mkdir(odex_oat_dir_.c_str(), 0700));

    odex_dir_ = odex_oat_dir_ + "/" + std::string(GetInstructionSetString(kRuntimeISA));
    ASSERT_EQ(0, mkdir(odex_dir_.c_str(), 0700));

    // Verify the environment is as we expect
    uint32_t checksum;
    std::string error_msg;
    ASSERT_TRUE(OS::FileExists(GetSystemImageFile().c_str()))
      << "Expected pre-compiled boot image to be at: " << GetSystemImageFile();
    ASSERT_TRUE(OS::FileExists(GetDexSrc1().c_str()))
      << "Expected dex file to be at: " << GetDexSrc1();
    ASSERT_TRUE(OS::FileExists(GetStrippedDexSrc1().c_str()))
      << "Expected stripped dex file to be at: " << GetStrippedDexSrc1();
    ASSERT_FALSE(DexFile::GetChecksum(GetStrippedDexSrc1().c_str(), &checksum, &error_msg))
      << "Expected stripped dex file to be stripped: " << GetStrippedDexSrc1();
    ASSERT_TRUE(OS::FileExists(GetDexSrc2().c_str()))
      << "Expected dex file to be at: " << GetDexSrc2();

    // GetMultiDexSrc2 should have the same primary dex checksum as
    // GetMultiDexSrc1, but a different secondary dex checksum.
    std::vector<std::unique_ptr<const DexFile>> multi1;
    ASSERT_TRUE(DexFile::Open(GetMultiDexSrc1().c_str(),
          GetMultiDexSrc1().c_str(), &error_msg, &multi1)) << error_msg;
    ASSERT_GT(multi1.size(), 1u);

    std::vector<std::unique_ptr<const DexFile>> multi2;
    ASSERT_TRUE(DexFile::Open(GetMultiDexSrc2().c_str(),
          GetMultiDexSrc2().c_str(), &error_msg, &multi2)) << error_msg;
    ASSERT_GT(multi2.size(), 1u);

    ASSERT_EQ(multi1[0]->GetLocationChecksum(), multi2[0]->GetLocationChecksum());
    ASSERT_NE(multi1[1]->GetLocationChecksum(), multi2[1]->GetLocationChecksum());
  }

  virtual void SetUpRuntimeOptions(RuntimeOptions* options) OVERRIDE {
    // options->push_back(std::make_pair("-verbose:oat", nullptr));

    // Set up the image location.
    options->push_back(std::make_pair("-Ximage:" + GetImageLocation(),
          nullptr));
    // Make sure compilercallbacks are not set so that relocation will be
    // enabled.
    callbacks_.reset();
  }

  virtual void TearDown() OVERRIDE {
    ClearDirectory(odex_dir_.c_str());
    ASSERT_EQ(0, rmdir(odex_dir_.c_str()));

    ClearDirectory(odex_oat_dir_.c_str());
    ASSERT_EQ(0, rmdir(odex_oat_dir_.c_str()));

    ClearDirectory(scratch_dir_.c_str());
    ASSERT_EQ(0, rmdir(scratch_dir_.c_str()));

    CommonRuntimeTest::TearDown();
  }

  static void Copy(const std::string& src, const std::string& dst) {
    std::ifstream  src_stream(src, std::ios::binary);
    std::ofstream  dst_stream(dst, std::ios::binary);

    dst_stream << src_stream.rdbuf();
  }

  // Returns the directory where the pre-compiled core.art can be found.
  // TODO: We should factor out this into common tests somewhere rather than
  // re-hardcoding it here (This was copied originally from the elf writer
  // test).
  std::string GetImageDirectory() const {
    if (IsHost()) {
      const char* host_dir = getenv("ANDROID_HOST_OUT");
      CHECK(host_dir != nullptr);
      return std::string(host_dir) + "/framework";
    } else {
      return std::string("/data/art-test");
    }
  }

  std::string GetImageLocation() const {
    return GetImageDirectory() + "/core.art";
  }

  std::string GetSystemImageFile() const {
    return GetImageDirectory() + "/" + GetInstructionSetString(kRuntimeISA)
      + "/core.art";
  }

  bool GetCachedImageFile(/*out*/std::string* image, std::string* error_msg) const {
    std::string cache = GetDalvikCache(GetInstructionSetString(kRuntimeISA), true);
    return GetDalvikCacheFilename(GetImageLocation().c_str(), cache.c_str(), image, error_msg);
  }

  std::string GetDexSrc1() const {
    return GetTestDexFileName("Main");
  }

  // Returns the path to a dex file equivalent to GetDexSrc1, but with the dex
  // file stripped.
  std::string GetStrippedDexSrc1() const {
    return GetTestDexFileName("MainStripped");
  }

  std::string GetMultiDexSrc1() const {
    return GetTestDexFileName("MultiDex");
  }

  // Returns the path to a multidex file equivalent to GetMultiDexSrc2, but
  // with the contents of the secondary dex file changed.
  std::string GetMultiDexSrc2() const {
    return GetTestDexFileName("MultiDexModifiedSecondary");
  }

  std::string GetDexSrc2() const {
    return GetTestDexFileName("Nested");
  }

  // Scratch directory, for dex and odex files (oat files will go in the
  // dalvik cache).
  const std::string& GetScratchDir() const {
    return scratch_dir_;
  }

  // Odex directory is the subdirectory in the scratch directory where odex
  // files should be located.
  const std::string& GetOdexDir() const {
    return odex_dir_;
  }

 private:
  std::string scratch_dir_;
  std::string odex_oat_dir_;
  std::string odex_dir_;
};

}  // namespace art

#endif  // ART_RUNTIME_DEX2OAT_ENVIRONMENT_TEST_H_
