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

#ifndef ART_RUNTIME_COMMON_RUNTIME_TEST_H_
#define ART_RUNTIME_COMMON_RUNTIME_TEST_H_

#include <gtest/gtest.h>
#include <jni.h>

#include <string>

#include "base/mutex.h"
#include "globals.h"
#include "os.h"

namespace art {

class ClassLinker;
class CompilerCallbacks;
class DexFile;
class JavaVMExt;
class Runtime;
typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;

class ScratchFile {
 public:
  ScratchFile();

  ScratchFile(const ScratchFile& other, const char* suffix);

  explicit ScratchFile(File* file);

  ~ScratchFile();

  const std::string& GetFilename() const {
    return filename_;
  }

  File* GetFile() const {
    return file_.get();
  }

  int GetFd() const;

  void Unlink();

 private:
  std::string filename_;
  std::unique_ptr<File> file_;
};

class CommonRuntimeTest : public testing::Test {
 public:
  static void SetUpAndroidRoot();

  // Note: setting up ANDROID_DATA may create a temporary directory. If this is used in a
  // non-derived class, be sure to also call the corresponding tear-down below.
  static void SetUpAndroidData(std::string& android_data);

  static void TearDownAndroidData(const std::string& android_data, bool fail_on_error);

  CommonRuntimeTest();
  ~CommonRuntimeTest();

 protected:
  static bool IsHost() {
    return !kIsTargetBuild;
  }

  const DexFile* LoadExpectSingleDexFile(const char* location);

  virtual void SetUp();

  // Allow subclases such as CommonCompilerTest to add extra options.
  virtual void SetUpRuntimeOptions(RuntimeOptions* options) {}

  void ClearDirectory(const char* dirpath);

  virtual void TearDown();

  // Gets the path of the libcore dex file.
  std::string GetLibCoreDexFileName();

  // Gets the path of the specified dex file for host or target.
  std::string GetDexFileName(const std::string& jar_prefix);

  std::string GetTestAndroidRoot();

  std::vector<const DexFile*> OpenTestDexFiles(const char* name)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  const DexFile* OpenTestDexFile(const char* name) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  jobject LoadDex(const char* dex_name) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  std::string android_data_;
  std::string dalvik_cache_;
  const DexFile* java_lang_dex_file_;  // owned by runtime_
  std::vector<const DexFile*> boot_class_path_;
  std::unique_ptr<Runtime> runtime_;
  // Owned by the runtime
  ClassLinker* class_linker_;

 private:
  std::unique_ptr<CompilerCallbacks> callbacks_;
  std::vector<const DexFile*> opened_dex_files_;
};

// Sets a CheckJni abort hook to catch failures. Note that this will cause CheckJNI to carry on
// rather than aborting, so be careful!
class CheckJniAbortCatcher {
 public:
  CheckJniAbortCatcher();

  ~CheckJniAbortCatcher();

  void Check(const char* expected_text);

 private:
  static void Hook(void* data, const std::string& reason);

  JavaVMExt* vm_;
  std::string actual_;

  DISALLOW_COPY_AND_ASSIGN(CheckJniAbortCatcher);
};

// TODO: These tests were disabled for portable when we went to having
// MCLinker link LLVM ELF output because we no longer just have code
// blobs in memory. We'll need to dlopen to load and relocate
// temporary output to resurrect these tests.
#define TEST_DISABLED_FOR_PORTABLE() \
  if (kUsePortableCompiler) { \
    printf("WARNING: TEST DISABLED FOR PORTABLE\n"); \
    return; \
  }

// TODO: When heap reference poisoning works with the compiler, get rid of this.
#define TEST_DISABLED_FOR_HEAP_REFERENCE_POISONING() \
  if (kPoisonHeapReferences) { \
    printf("WARNING: TEST DISABLED FOR HEAP REFERENCE POISONING\n"); \
    return; \
  }

#define TEST_DISABLED_FOR_MIPS() \
  if (kRuntimeISA == kMips || kRuntimeISA == kMips64) { \
    printf("WARNING: TEST DISABLED FOR MIPS\n"); \
    return; \
  }

}  // namespace art

namespace std {

// TODO: isn't gtest supposed to be able to print STL types for itself?
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& rhs);

}  // namespace std

#endif  // ART_RUNTIME_COMMON_RUNTIME_TEST_H_
