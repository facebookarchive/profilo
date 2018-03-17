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

#ifndef ART_RUNTIME_CLASS_LOADER_CONTEXT_H_
#define ART_RUNTIME_CLASS_LOADER_CONTEXT_H_

#include <string>
#include <vector>

#include "arch/instruction_set.h"
#include "base/dchecked_vector.h"
#include "handle_scope.h"
#include "mirror/class_loader.h"
#include "scoped_thread_state_change.h"

namespace art {

class DexFile;
class OatFile;

// Utility class which holds the class loader context used during compilation/verification.
class ClassLoaderContext {
 public:
  ~ClassLoaderContext();

  // Opens requested class path files and appends them to ClassLoaderInfo::opened_dex_files.
  // If the dex files have been stripped, the method opens them from their oat files which are added
  // to ClassLoaderInfo::opened_oat_files. The 'classpath_dir' argument specifies the directory to
  // use for the relative class paths.
  // Returns true if all dex files where successfully opened.
  // It may be called only once per ClassLoaderContext. Subsequent calls will return the same
  // result without doing anything.
  //
  // This will replace the class path locations with the locations of the opened dex files.
  // (Note that one dex file can contain multidexes. Each multidex will be added to the classpath
  // separately.)
  //
  // Note that a "false" return could mean that either an apk/jar contained no dex files or
  // that we hit a I/O or checksum mismatch error.
  // TODO(calin): Currently there's no easy way to tell the difference.
  //
  // TODO(calin): we're forced to complicate the flow in this class with a different
  // OpenDexFiles step because the current dex2oat flow requires the dex files be opened before
  // the class loader is created. Consider reworking the dex2oat part.
  bool OpenDexFiles(InstructionSet isa, const std::string& classpath_dir);

  // Remove the specified compilation sources from all classpaths present in this context.
  // Should only be called before the first call to OpenDexFiles().
  bool RemoveLocationsFromClassPaths(const dchecked_vector<std::string>& compilation_sources);

  // Creates the entire class loader hierarchy according to the current context.
  // Returns the first class loader from the chain.
  //
  // For example: if the context was built from the spec
  // "ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]..."
  // the method returns the class loader correponding to ClassLoader1. The parent chain will be
  // ClassLoader1 --> ClassLoader2 --> ... --> BootClassLoader.
  //
  // The compilation sources are appended to the classpath of the first class loader (in the above
  // example ClassLoader1).
  //
  // If the context is empty, this method only creates a single PathClassLoader with the
  // given compilation_sources.
  //
  // Notes:
  //   1) the objects are not completely set up. Do not use this outside of tests and the compiler.
  //   2) should only be called before the first call to OpenDexFiles().
  jobject CreateClassLoader(const std::vector<const DexFile*>& compilation_sources) const;

  // Encodes the context as a string suitable to be added in oat files.
  // (so that it can be read and verified at runtime against the actual class
  // loader hierarchy).
  // Should only be called if OpenDexFiles() returned true.
  // E.g. if the context is PCL[a.dex:b.dex] this will return
  // "PCL[a.dex*a_checksum*b.dex*a_checksum]".
  std::string EncodeContextForOatFile(const std::string& base_dir) const;

  // Encodes the context as a string suitable to be passed to dex2oat.
  // This is the same as EncodeContextForOatFile but without adding the checksums
  // and only adding each dex files once (no multidex).
  // Should only be called if OpenDexFiles() returned true.
  std::string EncodeContextForDex2oat(const std::string& base_dir) const;

  // Flattens the opened dex files into the given vector.
  // Should only be called if OpenDexFiles() returned true.
  std::vector<const DexFile*> FlattenOpenedDexFiles() const;

  // Verifies that the current context is identical to the context encoded as `context_spec`.
  // Identical means:
  //    - the number and type of the class loaders from the chain matches
  //    - the class loader from the same position have the same classpath
  //      (the order and checksum of the dex files matches)
  // This should be called after OpenDexFiles().
  bool VerifyClassLoaderContextMatch(const std::string& context_spec) const;

  // Creates the class loader context from the given string.
  // The format: ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]...
  // ClassLoaderType is either "PCL" (PathClassLoader) or "DLC" (DelegateLastClassLoader).
  // ClasspathElem is the path of dex/jar/apk file.
  //
  // The spec represents a class loader chain with the natural interpretation:
  // ClassLoader1 has ClassLoader2 as parent which has ClassLoader3 as a parent and so on.
  // The last class loader is assumed to have the BootClassLoader as a parent.
  //
  // Note that we allowed class loaders with an empty class path in order to support a custom
  // class loader for the source dex files.
  static std::unique_ptr<ClassLoaderContext> Create(const std::string& spec);

  // Creates a context for the given class_loader and dex_elements.
  // The method will walk the parent chain starting from `class_loader` and add their dex files
  // to the current class loaders chain. The `dex_elements` will be added at the end of the
  // classpath belonging to the `class_loader` argument.
  // The ownership of the opened dex files will be retained by the given `class_loader`.
  // If there are errors in processing the class loader chain (e.g. unsupported elements) the
  // method returns null.
  static std::unique_ptr<ClassLoaderContext> CreateContextForClassLoader(jobject class_loader,
                                                                         jobjectArray dex_elements);

  // Returns the default class loader context to be used when none is specified.
  // This will return a context with a single and empty PathClassLoader.
  static std::unique_ptr<ClassLoaderContext> Default();

 private:
  enum ClassLoaderType {
    kInvalidClassLoader = 0,
    kPathClassLoader = 1,
    kDelegateLastClassLoader = 2
  };

  struct ClassLoaderInfo {
    // The type of this class loader.
    ClassLoaderType type;
    // The list of class path elements that this loader loads.
    // Note that this list may contain relative paths.
    std::vector<std::string> classpath;
    // The list of class path elements checksums.
    // May be empty if the checksums are not given when the context is created.
    std::vector<uint32_t> checksums;
    // After OpenDexFiles is called this holds the opened dex files.
    std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
    // After OpenDexFiles, in case some of the dex files were opened from their oat files
    // this holds the list of opened oat files.
    std::vector<std::unique_ptr<OatFile>> opened_oat_files;

    explicit ClassLoaderInfo(ClassLoaderType cl_type) : type(cl_type) {}
  };

  // Creates an empty context (with no class loaders).
  ClassLoaderContext();

  // Constructs an empty context.
  // `owns_the_dex_files` specifies whether or not the context will own the opened dex files
  // present in the class loader chain. If `owns_the_dex_files` is true then OpenDexFiles cannot
  // be called on this context (dex_files_open_attempted_ and dex_files_open_result_ will be set
  // to true as well)
  explicit ClassLoaderContext(bool owns_the_dex_files);

  // Reads the class loader spec in place and returns true if the spec is valid and the
  // compilation context was constructed.
  bool Parse(const std::string& spec, bool parse_checksums = false);

  // Attempts to parse a single class loader spec for the given class_loader_type.
  // If successful the class loader spec will be added to the chain.
  // Returns whether or not the operation was successful.
  bool ParseClassLoaderSpec(const std::string& class_loader_spec,
                            ClassLoaderType class_loader_type,
                            bool parse_checksums = false);

  // CHECKs that the dex files were opened (OpenDexFiles was called and set dex_files_open_result_
  // to true). Aborts if not. The `calling_method` is used in the log message to identify the source
  // of the call.
  void CheckDexFilesOpened(const std::string& calling_method) const;

  // Adds the `class_loader` info to the context.
  // The dex file present in `dex_elements` array (if not null) will be added at the end of
  // the classpath.
  bool AddInfoToContextFromClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                       Handle<mirror::ClassLoader> class_loader,
                                       Handle<mirror::ObjectArray<mirror::Object>> dex_elements)
    REQUIRES_SHARED(Locks::mutator_lock_);

  // Encodes the context as a string suitable to be passed to dex2oat or to be added to the
  // oat file as the class path key.
  // If for_dex2oat is true, the encoding adds each file once (i.e. it does not add multidex
  // location). Otherwise, for oat files, the encoding adds all the dex files (including multidex)
  // together with their checksums.
  // Should only be called if OpenDexFiles() returned true.
  std::string EncodeContext(const std::string& base_dir, bool for_dex2oat) const;

  // Extracts the class loader type from the given spec.
  // Return ClassLoaderContext::kInvalidClassLoader if the class loader type is not
  // recognized.
  static ClassLoaderType ExtractClassLoaderType(const std::string& class_loader_spec);

  // Returns the string representation of the class loader type.
  // The returned format can be used when parsing a context spec.
  static const char* GetClassLoaderTypeName(ClassLoaderType type);

  // Returns the WellKnownClass for the given class loader type.
  static jclass GetClassLoaderClass(ClassLoaderType type);

  // The class loader chain represented as a vector.
  // The parent of class_loader_chain_[i] is class_loader_chain_[i++].
  // The parent of the last element is assumed to be the boot class loader.
  std::vector<ClassLoaderInfo> class_loader_chain_;

  // Whether or not the class loader context should be ignored at runtime when loading the oat
  // files. When true, dex2oat will use OatFile::kSpecialSharedLibrary as the classpath key in
  // the oat file.
  // TODO(calin): Can we get rid of this and cover all relevant use cases?
  // (e.g. packages using prebuild system packages as shared libraries b/36480683)
  bool special_shared_library_;

  // Whether or not OpenDexFiles() was called.
  bool dex_files_open_attempted_;
  // The result of the last OpenDexFiles() operation.
  bool dex_files_open_result_;

  // Whether or not the context owns the opened dex and oat files.
  // If true, the opened dex files will be de-allocated when the context is destructed.
  // If false, the objects will continue to be alive.
  // Note that for convenience the the opened dex/oat files are stored as unique pointers
  // which will release their ownership in the destructor based on this flag.
  const bool owns_the_dex_files_;

  friend class ClassLoaderContextTest;

  DISALLOW_COPY_AND_ASSIGN(ClassLoaderContext);
};

}  // namespace art
#endif  // ART_RUNTIME_CLASS_LOADER_CONTEXT_H_
