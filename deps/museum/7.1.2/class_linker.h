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

#ifndef ART_RUNTIME_CLASS_LINKER_H_
#define ART_RUNTIME_CLASS_LINKER_H_

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/allocator.h"
#include "base/hash_set.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "class_table.h"
#include "dex_cache_resolved_classes.h"
#include "dex_file.h"
#include "gc_root.h"
#include "jni.h"
#include "oat_file.h"
#include "object_callbacks.h"

namespace art {

namespace gc {
namespace space {
  class ImageSpace;
}  // namespace space
}  // namespace gc
namespace mirror {
  class ClassLoader;
  class DexCache;
  class DexCachePointerArray;
  class DexCacheTest_Open_Test;
  class IfTable;
  template<class T> class ObjectArray;
  class StackTraceElement;
}  // namespace mirror

class ImtConflictTable;
template<class T> class Handle;
template<class T> class MutableHandle;
class InternTable;
template<class T> class ObjectLock;
class Runtime;
class ScopedObjectAccessAlreadyRunnable;
template<size_t kNumReferences> class PACKED(4) StackHandleScope;

enum VisitRootFlags : uint8_t;

class ClassVisitor {
 public:
  virtual ~ClassVisitor() {}
  // Return true to continue visiting.
  virtual bool operator()(mirror::Class* klass) = 0;
};

class ClassLoaderVisitor {
 public:
  virtual ~ClassLoaderVisitor() {}
  virtual void Visit(mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::classlinker_classes_lock_, Locks::mutator_lock_) = 0;
};

class ClassLinker {
 public:
  // Well known mirror::Class roots accessed via GetClassRoot.
  enum ClassRoot {
    kJavaLangClass,
    kJavaLangObject,
    kClassArrayClass,
    kObjectArrayClass,
    kJavaLangString,
    kJavaLangDexCache,
    kJavaLangRefReference,
    kJavaLangReflectConstructor,
    kJavaLangReflectField,
    kJavaLangReflectMethod,
    kJavaLangReflectProxy,
    kJavaLangStringArrayClass,
    kJavaLangReflectConstructorArrayClass,
    kJavaLangReflectFieldArrayClass,
    kJavaLangReflectMethodArrayClass,
    kJavaLangClassLoader,
    kJavaLangThrowable,
    kJavaLangClassNotFoundException,
    kJavaLangStackTraceElement,
    kPrimitiveBoolean,
    kPrimitiveByte,
    kPrimitiveChar,
    kPrimitiveDouble,
    kPrimitiveFloat,
    kPrimitiveInt,
    kPrimitiveLong,
    kPrimitiveShort,
    kPrimitiveVoid,
    kBooleanArrayClass,
    kByteArrayClass,
    kCharArrayClass,
    kDoubleArrayClass,
    kFloatArrayClass,
    kIntArrayClass,
    kLongArrayClass,
    kShortArrayClass,
    kJavaLangStackTraceElementArrayClass,
    kClassRootsMax,
  };

  explicit ClassLinker(InternTable* intern_table);
  ~ClassLinker();

  // Initialize class linker by bootstraping from dex files.
  bool InitWithoutImage(std::vector<std::unique_ptr<const DexFile>> boot_class_path,
                        std::string* error_msg)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Initialize class linker from one or more boot images.
  bool InitFromBootImage(std::string* error_msg)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Add an image space to the class linker, may fix up classloader fields and dex cache fields.
  // The dex files that were newly opened for the space are placed in the out argument
  // out_dex_files. Returns true if the operation succeeded.
  // The space must be already added to the heap before calling AddImageSpace since we need to
  // properly handle read barriers and object marking.
  bool AddImageSpace(gc::space::ImageSpace* space,
                     Handle<mirror::ClassLoader> class_loader,
                     jobjectArray dex_elements,
                     const char* dex_location,
                     std::vector<std::unique_ptr<const DexFile>>* out_dex_files,
                     std::string* error_msg)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool OpenImageDexFiles(gc::space::ImageSpace* space,
                         std::vector<std::unique_ptr<const DexFile>>* out_dex_files,
                         std::string* error_msg)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Finds a class by its descriptor, loading it if necessary.
  // If class_loader is null, searches boot_class_path_.
  mirror::Class* FindClass(Thread* self,
                           const char* descriptor,
                           Handle<mirror::ClassLoader> class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Finds a class in the path class loader, loading it if necessary without using JNI. Hash
  // function is supposed to be ComputeModifiedUtf8Hash(descriptor). Returns true if the
  // class-loader chain could be handled, false otherwise, i.e., a non-supported class-loader
  // was encountered while walking the parent chain (currently only BootClassLoader and
  // PathClassLoader are supported).
  bool FindClassInPathClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                  Thread* self,
                                  const char* descriptor,
                                  size_t hash,
                                  Handle<mirror::ClassLoader> class_loader,
                                  mirror::Class** result)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Finds a class by its descriptor using the "system" class loader, ie by searching the
  // boot_class_path_.
  mirror::Class* FindSystemClass(Thread* self, const char* descriptor)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Finds the array class given for the element class.
  mirror::Class* FindArrayClass(Thread* self, mirror::Class** element_class)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Returns true if the class linker is initialized.
  bool IsInitialized() const {
    return init_done_;
  }

  // Define a new a class based on a ClassDef from a DexFile
  mirror::Class* DefineClass(Thread* self,
                             const char* descriptor,
                             size_t hash,
                             Handle<mirror::ClassLoader> class_loader,
                             const DexFile& dex_file,
                             const DexFile::ClassDef& dex_class_def)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Finds a class by its descriptor, returning null if it isn't wasn't loaded
  // by the given 'class_loader'.
  mirror::Class* LookupClass(Thread* self,
                             const char* descriptor,
                             size_t hash,
                             mirror::ClassLoader* class_loader)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Finds all the classes with the given descriptor, regardless of ClassLoader.
  void LookupClasses(const char* descriptor, std::vector<mirror::Class*>& classes)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::Class* FindPrimitiveClass(char type) SHARED_REQUIRES(Locks::mutator_lock_);

  // General class unloading is not supported, this is used to prune
  // unwanted classes during image writing.
  bool RemoveClass(const char* descriptor, mirror::ClassLoader* class_loader)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void DumpAllClasses(int flags)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void DumpForSigQuit(std::ostream& os) REQUIRES(!Locks::classlinker_classes_lock_);

  size_t NumLoadedClasses()
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Resolve a String with the given index from the DexFile, storing the
  // result in the DexCache. The referrer is used to identify the
  // target DexCache and ClassLoader to use for resolution.
  mirror::String* ResolveString(uint32_t string_idx, ArtMethod* referrer)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Resolve a String with the given index from the DexFile, storing the
  // result in the DexCache.
  mirror::String* ResolveString(const DexFile& dex_file, uint32_t string_idx,
                                Handle<mirror::DexCache> dex_cache)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Find a String with the given index from the DexFile, storing the
  // result in the DexCache if found. Return null if not found.
  mirror::String* LookupString(const DexFile& dex_file, uint32_t string_idx,
                               Handle<mirror::DexCache> dex_cache)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Resolve a Type with the given index from the DexFile, storing the
  // result in the DexCache. The referrer is used to identity the
  // target DexCache and ClassLoader to use for resolution.
  mirror::Class* ResolveType(const DexFile& dex_file, uint16_t type_idx, mirror::Class* referrer)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Resolve a Type with the given index from the DexFile, storing the
  // result in the DexCache. The referrer is used to identify the
  // target DexCache and ClassLoader to use for resolution.
  mirror::Class* ResolveType(uint16_t type_idx, ArtMethod* referrer)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  mirror::Class* ResolveType(uint16_t type_idx, ArtField* referrer)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Resolve a type with the given ID from the DexFile, storing the
  // result in DexCache. The ClassLoader is used to search for the
  // type, since it may be referenced from but not contained within
  // the given DexFile.
  mirror::Class* ResolveType(const DexFile& dex_file,
                             uint16_t type_idx,
                             Handle<mirror::DexCache> dex_cache,
                             Handle<mirror::ClassLoader> class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Determine whether a dex cache result should be trusted, or an IncompatibleClassChangeError
  // check should be performed even after a hit.
  enum ResolveMode {  // private.
    kNoICCECheckForCache,
    kForceICCECheck
  };

  // Resolve a method with a given ID from the DexFile, storing the
  // result in DexCache. The ClassLinker and ClassLoader are used as
  // in ResolveType. What is unique is the method type argument which
  // is used to determine if this method is a direct, static, or
  // virtual method.
  template <ResolveMode kResolveMode>
  ArtMethod* ResolveMethod(const DexFile& dex_file,
                           uint32_t method_idx,
                           Handle<mirror::DexCache> dex_cache,
                           Handle<mirror::ClassLoader> class_loader,
                           ArtMethod* referrer,
                           InvokeType type)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  ArtMethod* GetResolvedMethod(uint32_t method_idx, ArtMethod* referrer)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // This returns the class referred to by GetMethodId(method_idx).class_idx_. This might be
  // different then the declaring class of the resolved method due to copied
  // miranda/default/conflict methods.
  mirror::Class* ResolveReferencedClassOfMethod(uint32_t method_idx,
                                                Handle<mirror::DexCache> dex_cache,
                                                Handle<mirror::ClassLoader> class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);
  template <ResolveMode kResolveMode>
  ArtMethod* ResolveMethod(Thread* self, uint32_t method_idx, ArtMethod* referrer, InvokeType type)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);
  ArtMethod* ResolveMethodWithoutInvokeType(const DexFile& dex_file,
                                            uint32_t method_idx,
                                            Handle<mirror::DexCache> dex_cache,
                                            Handle<mirror::ClassLoader> class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  ArtField* GetResolvedField(uint32_t field_idx, mirror::Class* field_declaring_class)
      SHARED_REQUIRES(Locks::mutator_lock_);
  ArtField* GetResolvedField(uint32_t field_idx, mirror::DexCache* dex_cache)
      SHARED_REQUIRES(Locks::mutator_lock_);
  ArtField* ResolveField(uint32_t field_idx, ArtMethod* referrer, bool is_static)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Resolve a field with a given ID from the DexFile, storing the
  // result in DexCache. The ClassLinker and ClassLoader are used as
  // in ResolveType. What is unique is the is_static argument which is
  // used to determine if we are resolving a static or non-static
  // field.
  ArtField* ResolveField(const DexFile& dex_file, uint32_t field_idx,
                         Handle<mirror::DexCache> dex_cache,
                         Handle<mirror::ClassLoader> class_loader, bool is_static)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Resolve a field with a given ID from the DexFile, storing the
  // result in DexCache. The ClassLinker and ClassLoader are used as
  // in ResolveType. No is_static argument is provided so that Java
  // field resolution semantics are followed.
  ArtField* ResolveFieldJLS(const DexFile& dex_file,
                            uint32_t field_idx,
                            Handle<mirror::DexCache> dex_cache,
                            Handle<mirror::ClassLoader> class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Get shorty from method index without resolution. Used to do handlerization.
  const char* MethodShorty(uint32_t method_idx, ArtMethod* referrer, uint32_t* length)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns true on success, false if there's an exception pending.
  // can_run_clinit=false allows the compiler to attempt to init a class,
  // given the restriction that no <clinit> execution is possible.
  bool EnsureInitialized(Thread* self,
                         Handle<mirror::Class> c,
                         bool can_init_fields,
                         bool can_init_parents)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // Initializes classes that have instances in the image but that have
  // <clinit> methods so they could not be initialized by the compiler.
  void RunRootClinits()
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  mirror::DexCache* RegisterDexFile(const DexFile& dex_file,
                                    mirror::ClassLoader* class_loader)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void RegisterDexFile(const DexFile& dex_file, Handle<mirror::DexCache> dex_cache)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  const std::vector<const DexFile*>& GetBootClassPath() {
    return boot_class_path_;
  }

  void VisitClasses(ClassVisitor* visitor)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Less efficient variant of VisitClasses that copies the class_table_ into secondary storage
  // so that it can visit individual classes without holding the doesn't hold the
  // Locks::classlinker_classes_lock_. As the Locks::classlinker_classes_lock_ isn't held this code
  // can race with insertion and deletion of classes while the visitor is being called.
  void VisitClassesWithoutClassesLock(ClassVisitor* visitor)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  void VisitClassRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void VisitRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::DexCache* FindDexCache(Thread* self,
                                 const DexFile& dex_file,
                                 bool allow_failure = false)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void FixupDexCaches(ArtMethod* resolution_method)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Allocate an instance of a java.lang.Object.
  mirror::Object* AllocObject(Thread* self)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  // TODO: replace this with multiple methods that allocate the correct managed type.
  template <class T>
  mirror::ObjectArray<T>* AllocObjectArray(Thread* self, size_t length)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  mirror::ObjectArray<mirror::Class>* AllocClassArray(Thread* self, size_t length)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  mirror::ObjectArray<mirror::String>* AllocStringArray(Thread* self, size_t length)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  LengthPrefixedArray<ArtField>* AllocArtFieldArray(Thread* self,
                                                    LinearAlloc* allocator,
                                                    size_t length);

  LengthPrefixedArray<ArtMethod>* AllocArtMethodArray(Thread* self,
                                                      LinearAlloc* allocator,
                                                      size_t length);

  mirror::PointerArray* AllocPointerArray(Thread* self, size_t length)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  mirror::IfTable* AllocIfTable(Thread* self, size_t ifcount)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  mirror::ObjectArray<mirror::StackTraceElement>* AllocStackTraceElementArray(Thread* self,
                                                                              size_t length)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  void VerifyClass(Thread* self,
                   Handle<mirror::Class> klass,
                   LogSeverity log_level = LogSeverity::NONE)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);
  bool VerifyClassUsingOatFile(const DexFile& dex_file,
                               mirror::Class* klass,
                               mirror::Class::Status& oat_file_class_status)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);
  void ResolveClassExceptionHandlerTypes(Handle<mirror::Class> klass)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);
  void ResolveMethodExceptionHandlerTypes(ArtMethod* klass)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  mirror::Class* CreateProxyClass(ScopedObjectAccessAlreadyRunnable& soa,
                                  jstring name,
                                  jobjectArray interfaces,
                                  jobject loader,
                                  jobjectArray methods,
                                  jobjectArray throws)
      SHARED_REQUIRES(Locks::mutator_lock_);
  std::string GetDescriptorForProxy(mirror::Class* proxy_class)
      SHARED_REQUIRES(Locks::mutator_lock_);
  ArtMethod* FindMethodForProxy(mirror::Class* proxy_class, ArtMethod* proxy_method)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Get the oat code for a method when its class isn't yet initialized
  const void* GetQuickOatCodeFor(ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Get compiled code for a method, return null if no code
  // exists. This is unlike Get..OatCodeFor which will return a bridge
  // or interpreter entrypoint.
  const void* GetOatMethodQuickCodeFor(ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  const OatFile::OatMethod FindOatMethodFor(ArtMethod* method, bool* found)
      SHARED_REQUIRES(Locks::mutator_lock_);

  pid_t GetClassesLockOwner();  // For SignalCatcher.
  pid_t GetDexLockOwner();  // For SignalCatcher.

  mirror::Class* GetClassRoot(ClassRoot class_root) SHARED_REQUIRES(Locks::mutator_lock_);

  static const char* GetClassRootDescriptor(ClassRoot class_root);

  // Is the given entry point quick code to run the resolution stub?
  bool IsQuickResolutionStub(const void* entry_point) const;

  // Is the given entry point quick code to bridge into the interpreter?
  bool IsQuickToInterpreterBridge(const void* entry_point) const;

  // Is the given entry point quick code to run the generic JNI stub?
  bool IsQuickGenericJniStub(const void* entry_point) const;

  InternTable* GetInternTable() const {
    return intern_table_;
  }

  // Set the entrypoints up for method to the given code.
  void SetEntryPointsToCompiledCode(ArtMethod* method, const void* method_code) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Set the entrypoints up for method to the enter the interpreter.
  void SetEntryPointsToInterpreter(ArtMethod* method) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Attempts to insert a class into a class table.  Returns null if
  // the class was inserted, otherwise returns an existing class with
  // the same descriptor and ClassLoader.
  mirror::Class* InsertClass(const char* descriptor, mirror::Class* klass, size_t hash)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::ObjectArray<mirror::Class>* GetClassRoots() SHARED_REQUIRES(Locks::mutator_lock_) {
    mirror::ObjectArray<mirror::Class>* class_roots = class_roots_.Read();
    DCHECK(class_roots != nullptr);
    return class_roots;
  }

  // Move all of the boot image classes into the class table for faster lookups.
  void AddBootImageClassesToClassTable()
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Add image classes to the class table.
  void AddImageClassesToClassTable(std::vector<gc::space::ImageSpace*> image_spaces,
                                   mirror::ClassLoader* class_loader)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Move the class table to the pre-zygote table to reduce memory usage. This works by ensuring
  // that no more classes are ever added to the pre zygote table which makes it that the pages
  // always remain shared dirty instead of private dirty.
  void MoveClassTableToPreZygote()
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Creates a GlobalRef PathClassLoader that can be used to load classes from the given dex files.
  // Note: the objects are not completely set up. Do not use this outside of tests and the compiler.
  jobject CreatePathClassLoader(Thread* self, const std::vector<const DexFile*>& dex_files)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  size_t GetImagePointerSize() const {
    DCHECK(ValidPointerSize(image_pointer_size_)) << image_pointer_size_;
    return image_pointer_size_;
  }

  // Used by image writer for checking.
  bool ClassInClassTable(mirror::Class* klass)
      REQUIRES(Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ArtMethod* CreateRuntimeMethod(LinearAlloc* linear_alloc);

  // Clear the ArrayClass cache. This is necessary when cleaning up for the image, as the cache
  // entries are roots, but potentially not image classes.
  void DropFindArrayClassCache() SHARED_REQUIRES(Locks::mutator_lock_);

  // Clean up class loaders, this needs to happen after JNI weak globals are cleared.
  void CleanupClassLoaders()
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Unlike GetOrCreateAllocatorForClassLoader, GetAllocatorForClassLoader asserts that the
  // allocator for this class loader is already created.
  LinearAlloc* GetAllocatorForClassLoader(mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Return the linear alloc for a class loader if it is already allocated, otherwise allocate and
  // set it. TODO: Consider using a lock other than classlinker_classes_lock_.
  LinearAlloc* GetOrCreateAllocatorForClassLoader(mirror::ClassLoader* class_loader)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // May be called with null class_loader due to legacy code. b/27954959
  void InsertDexFileInToClassLoader(mirror::Object* dex_file, mirror::ClassLoader* class_loader)
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static bool ShouldUseInterpreterEntrypoint(ArtMethod* method, const void* quick_code)
      SHARED_REQUIRES(Locks::mutator_lock_);

  std::set<DexCacheResolvedClasses> GetResolvedClasses(bool ignore_boot_classes)
      REQUIRES(!dex_lock_);

  std::unordered_set<std::string> GetClassDescriptorsForProfileKeys(
      const std::set<DexCacheResolvedClasses>& classes)
      REQUIRES(!dex_lock_);

  static bool IsBootClassLoader(ScopedObjectAccessAlreadyRunnable& soa,
                                mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_);

  ArtMethod* AddMethodToConflictTable(mirror::Class* klass,
                                      ArtMethod* conflict_method,
                                      ArtMethod* interface_method,
                                      ArtMethod* method,
                                      bool force_new_conflict_method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Create a conflict table with a specified capacity.
  ImtConflictTable* CreateImtConflictTable(size_t count, LinearAlloc* linear_alloc);

  // Static version for when the class linker is not yet created.
  static ImtConflictTable* CreateImtConflictTable(size_t count,
                                                  LinearAlloc* linear_alloc,
                                                  size_t pointer_size);


  // Create the IMT and conflict tables for a class.
  void FillIMTAndConflictTables(mirror::Class* klass) SHARED_REQUIRES(Locks::mutator_lock_);

  // Clear class table strong roots (other than classes themselves). This is done by dex2oat to
  // allow pruning dex caches.
  void ClearClassTableStrongRoots() const
      REQUIRES(!Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Throw the class initialization failure recorded when first trying to initialize the given
  // class.
  void ThrowEarlierClassFailure(mirror::Class* c, bool wrap_in_no_class_def = false)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Get the actual holding class for a copied method. Pretty slow, don't call often.
  mirror::Class* GetHoldingClassOfCopiedMethod(ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  struct DexCacheData {
    // Weak root to the DexCache. Note: Do not decode this unnecessarily or else class unloading may
    // not work properly.
    jweak weak_root;
    // The following two fields are caches to the DexCache's fields and here to avoid unnecessary
    // jweak decode that triggers read barriers (and mark them alive unnecessarily and mess with
    // class unloading.)
    const DexFile* dex_file;
    GcRoot<mirror::Class>* resolved_types;
  };

 private:
  struct ClassLoaderData {
    jweak weak_root;  // Weak root to enable class unloading.
    ClassTable* class_table;
    LinearAlloc* allocator;
  };

  // Ensures that the supertype of 'klass' ('supertype') is verified. Returns false and throws
  // appropriate exceptions if verification failed hard. Returns true for successful verification or
  // soft-failures.
  bool AttemptSupertypeVerification(Thread* self,
                                    Handle<mirror::Class> klass,
                                    Handle<mirror::Class> supertype)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  static void DeleteClassLoader(Thread* self, const ClassLoaderData& data)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void VisitClassLoaders(ClassLoaderVisitor* visitor) const
      SHARED_REQUIRES(Locks::classlinker_classes_lock_, Locks::mutator_lock_);

  void VisitClassesInternal(ClassVisitor* visitor)
      SHARED_REQUIRES(Locks::classlinker_classes_lock_, Locks::mutator_lock_);

  // Returns the number of zygote and image classes.
  size_t NumZygoteClasses() const
      REQUIRES(Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Returns the number of non zygote nor image classes.
  size_t NumNonZygoteClasses() const
      REQUIRES(Locks::classlinker_classes_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void FinishInit(Thread* self)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  // For early bootstrapping by Init
  mirror::Class* AllocClass(Thread* self, mirror::Class* java_lang_Class, uint32_t class_size)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  // Alloc* convenience functions to avoid needing to pass in mirror::Class*
  // values that are known to the ClassLinker such as
  // kObjectArrayClass and kJavaLangString etc.
  mirror::Class* AllocClass(Thread* self, uint32_t class_size)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);
  mirror::DexCache* AllocDexCache(Thread* self,
                                  const DexFile& dex_file,
                                  LinearAlloc* linear_alloc)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  mirror::Class* CreatePrimitiveClass(Thread* self, Primitive::Type type)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);
  mirror::Class* InitializePrimitiveClass(mirror::Class* primitive_class, Primitive::Type type)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  mirror::Class* CreateArrayClass(Thread* self,
                                  const char* descriptor,
                                  size_t hash,
                                  Handle<mirror::ClassLoader> class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_, !Roles::uninterruptible_);

  void AppendToBootClassPath(Thread* self, const DexFile& dex_file)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);
  void AppendToBootClassPath(const DexFile& dex_file, Handle<mirror::DexCache> dex_cache)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  // Precomputes size needed for Class, in the case of a non-temporary class this size must be
  // sufficient to hold all static fields.
  uint32_t SizeOfClassWithoutEmbeddedTables(const DexFile& dex_file,
                                            const DexFile::ClassDef& dex_class_def);

  // Setup the classloader, class def index, type idx so that we can insert this class in the class
  // table.
  void SetupClass(const DexFile& dex_file,
                  const DexFile::ClassDef& dex_class_def,
                  Handle<mirror::Class> klass,
                  mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void LoadClass(Thread* self,
                 const DexFile& dex_file,
                 const DexFile::ClassDef& dex_class_def,
                 Handle<mirror::Class> klass)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void LoadClassMembers(Thread* self,
                        const DexFile& dex_file,
                        const uint8_t* class_data,
                        Handle<mirror::Class> klass,
                        const OatFile::OatClass* oat_class)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void LoadField(const ClassDataItemIterator& it, Handle<mirror::Class> klass, ArtField* dst)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void LoadMethod(Thread* self,
                  const DexFile& dex_file,
                  const ClassDataItemIterator& it,
                  Handle<mirror::Class> klass, ArtMethod* dst)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void FixupStaticTrampolines(mirror::Class* klass) SHARED_REQUIRES(Locks::mutator_lock_);

  // Finds the associated oat class for a dex_file and descriptor. Returns an invalid OatClass on
  // error and sets found to false.
  OatFile::OatClass FindOatClass(const DexFile& dex_file, uint16_t class_def_idx, bool* found)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void RegisterDexFileLocked(const DexFile& dex_file, Handle<mirror::DexCache> dex_cache)
      REQUIRES(dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  mirror::DexCache* FindDexCacheLocked(Thread* self, const DexFile& dex_file, bool allow_failure)
      REQUIRES(dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool InitializeClass(Thread* self,
                       Handle<mirror::Class> klass,
                       bool can_run_clinit,
                       bool can_init_parents)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);
  bool InitializeDefaultInterfaceRecursive(Thread* self,
                                           Handle<mirror::Class> klass,
                                           bool can_run_clinit,
                                           bool can_init_parents)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool WaitForInitializeClass(Handle<mirror::Class> klass,
                              Thread* self,
                              ObjectLock<mirror::Class>& lock);
  bool ValidateSuperClassDescriptors(Handle<mirror::Class> klass)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsSameDescriptorInDifferentClassContexts(Thread* self,
                                                const char* descriptor,
                                                Handle<mirror::ClassLoader> class_loader1,
                                                Handle<mirror::ClassLoader> class_loader2)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool IsSameMethodSignatureInDifferentClassContexts(Thread* self,
                                                     ArtMethod* method,
                                                     mirror::Class* klass1,
                                                     mirror::Class* klass2)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool LinkClass(Thread* self,
                 const char* descriptor,
                 Handle<mirror::Class> klass,
                 Handle<mirror::ObjectArray<mirror::Class>> interfaces,
                 MutableHandle<mirror::Class>* h_new_class_out)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::classlinker_classes_lock_);

  bool LinkSuperClass(Handle<mirror::Class> klass)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool LoadSuperAndInterfaces(Handle<mirror::Class> klass, const DexFile& dex_file)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  bool LinkMethods(Thread* self,
                   Handle<mirror::Class> klass,
                   Handle<mirror::ObjectArray<mirror::Class>> interfaces,
                   bool* out_new_conflict,
                   ArtMethod** out_imt)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Does anything needed to make sure that the compiler will not generate a direct invoke to this
  // method. Should only be called on non-invokable methods.
  void EnsureThrowsInvocationError(ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // A wrapper class representing the result of a method translation used for linking methods and
  // updating superclass default methods. For each method in a classes vtable there are 4 states it
  // could be in:
  // 1) No translation is necessary. In this case there is no MethodTranslation object for it. This
  //    is the standard case and is true when the method is not overridable by a default method,
  //    the class defines a concrete implementation of the method, the default method implementation
  //    remains the same, or an abstract method stayed abstract.
  // 2) The method must be translated to a different default method. We note this with
  //    CreateTranslatedMethod.
  // 3) The method must be replaced with a conflict method. This happens when a superclass
  //    implements an interface with a default method and this class implements an unrelated
  //    interface that also defines that default method. We note this with CreateConflictingMethod.
  // 4) The method must be replaced with an abstract miranda method. This happens when a superclass
  //    implements an interface with a default method and this class implements a subinterface of
  //    the superclass's interface which declares the default method abstract. We note this with
  //    CreateAbstractMethod.
  //
  // When a method translation is unnecessary (case #1), we don't put it into the
  // default_translation maps. So an instance of MethodTranslation must be in one of #2-#4.
  class MethodTranslation {
   public:
    // This slot must become a default conflict method.
    static MethodTranslation CreateConflictingMethod() {
      return MethodTranslation(Type::kConflict, /*translation*/nullptr);
    }

    // This slot must become an abstract method.
    static MethodTranslation CreateAbstractMethod() {
      return MethodTranslation(Type::kAbstract, /*translation*/nullptr);
    }

    // Use the given method as the current value for this vtable slot during translation.
    static MethodTranslation CreateTranslatedMethod(ArtMethod* new_method) {
      return MethodTranslation(Type::kTranslation, new_method);
    }

    // Returns true if this is a method that must become a conflict method.
    bool IsInConflict() const {
      return type_ == Type::kConflict;
    }

    // Returns true if this is a method that must become an abstract method.
    bool IsAbstract() const {
      return type_ == Type::kAbstract;
    }

    // Returns true if this is a method that must become a different method.
    bool IsTranslation() const {
      return type_ == Type::kTranslation;
    }

    // Get the translated version of this method.
    ArtMethod* GetTranslation() const {
      DCHECK(IsTranslation());
      DCHECK(translation_ != nullptr);
      return translation_;
    }

   private:
    enum class Type {
      kTranslation,
      kConflict,
      kAbstract,
    };

    MethodTranslation(Type type, ArtMethod* translation)
        : translation_(translation), type_(type) {}

    ArtMethod* const translation_;
    const Type type_;
  };

  // Links the virtual methods for the given class and records any default methods that will need to
  // be updated later.
  //
  // Arguments:
  // * self - The current thread.
  // * klass - class, whose vtable will be filled in.
  // * default_translations - Vtable index to new method map.
  //                          Any vtable entries that need to be updated with new default methods
  //                          are stored into the default_translations map. The default_translations
  //                          map is keyed on the vtable index that needs to be updated. We use this
  //                          map because if we override a default method with another default
  //                          method we need to update the vtable to point to the new method.
  //                          Unfortunately since we copy the ArtMethod* we cannot just do a simple
  //                          scan, we therefore store the vtable index's that might need to be
  //                          updated with the method they will turn into.
  // TODO This whole default_translations thing is very dirty. There should be a better way.
  bool LinkVirtualMethods(
        Thread* self,
        Handle<mirror::Class> klass,
        /*out*/std::unordered_map<size_t, MethodTranslation>* default_translations)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Sets up the interface lookup table (IFTable) in the correct order to allow searching for
  // default methods.
  bool SetupInterfaceLookupTable(Thread* self,
                                 Handle<mirror::Class> klass,
                                 Handle<mirror::ObjectArray<mirror::Class>> interfaces)
      SHARED_REQUIRES(Locks::mutator_lock_);


  enum class DefaultMethodSearchResult {
    kDefaultFound,
    kAbstractFound,
    kDefaultConflict
  };

  // Find the default method implementation for 'interface_method' in 'klass', if one exists.
  //
  // Arguments:
  // * self - The current thread.
  // * target_method - The method we are trying to find a default implementation for.
  // * klass - The class we are searching for a definition of target_method.
  // * out_default_method - The pointer we will store the found default method to on success.
  //
  // Return value:
  // * kDefaultFound - There were no conflicting method implementations found in the class while
  //                   searching for target_method. The default method implementation is stored into
  //                   out_default_method.
  // * kAbstractFound - There were no conflicting method implementations found in the class while
  //                   searching for target_method but no default implementation was found either.
  //                   out_default_method is set to null and the method should be considered not
  //                   implemented.
  // * kDefaultConflict - Conflicting method implementations were found when searching for
  //                      target_method. The value of *out_default_method is null.
  DefaultMethodSearchResult FindDefaultMethodImplementation(
      Thread* self,
      ArtMethod* target_method,
      Handle<mirror::Class> klass,
      /*out*/ArtMethod** out_default_method) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Sets the imt entries and fixes up the vtable for the given class by linking all the interface
  // methods. See LinkVirtualMethods for an explanation of what default_translations is.
  bool LinkInterfaceMethods(
      Thread* self,
      Handle<mirror::Class> klass,
      const std::unordered_map<size_t, MethodTranslation>& default_translations,
      bool* out_new_conflict,
      ArtMethod** out_imt)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool LinkStaticFields(Thread* self, Handle<mirror::Class> klass, size_t* class_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool LinkInstanceFields(Thread* self, Handle<mirror::Class> klass)
      SHARED_REQUIRES(Locks::mutator_lock_);
  bool LinkFields(Thread* self, Handle<mirror::Class> klass, bool is_static, size_t* class_size)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void LinkCode(ArtMethod* method,
                const OatFile::OatClass* oat_class,
                uint32_t class_def_method_index)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void CreateReferenceInstanceOffsets(Handle<mirror::Class> klass)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void CheckProxyConstructor(ArtMethod* constructor) const
      SHARED_REQUIRES(Locks::mutator_lock_);
  void CheckProxyMethod(ArtMethod* method, ArtMethod* prototype) const
      SHARED_REQUIRES(Locks::mutator_lock_);

  // For use by ImageWriter to find DexCaches for its roots
  ReaderWriterMutex* DexLock()
      SHARED_REQUIRES(Locks::mutator_lock_)
      LOCK_RETURNED(dex_lock_) {
    return &dex_lock_;
  }
  size_t GetDexCacheCount() SHARED_REQUIRES(Locks::mutator_lock_, dex_lock_) {
    return dex_caches_.size();
  }
  const std::list<DexCacheData>& GetDexCachesData()
      SHARED_REQUIRES(Locks::mutator_lock_, dex_lock_) {
    return dex_caches_;
  }

  void CreateProxyConstructor(Handle<mirror::Class> klass, ArtMethod* out)
      SHARED_REQUIRES(Locks::mutator_lock_);
  void CreateProxyMethod(Handle<mirror::Class> klass, ArtMethod* prototype, ArtMethod* out)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Ensures that methods have the kAccSkipAccessChecks bit set. We use the
  // kAccVerificationAttempted bit on the class access flags to determine whether this has been done
  // before.
  void EnsureSkipAccessChecksMethods(Handle<mirror::Class> c)
      SHARED_REQUIRES(Locks::mutator_lock_);

  mirror::Class* LookupClassFromBootImage(const char* descriptor)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Register a class loader and create its class table and allocator. Should not be called if
  // these are already created.
  void RegisterClassLoader(mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::classlinker_classes_lock_);

  // Returns null if not found.
  ClassTable* ClassTableForClassLoader(mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Insert a new class table if not found.
  ClassTable* InsertClassTableForClassLoader(mirror::ClassLoader* class_loader)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::classlinker_classes_lock_);

  // EnsureResolved is called to make sure that a class in the class_table_ has been resolved
  // before returning it to the caller. Its the responsibility of the thread that placed the class
  // in the table to make it resolved. The thread doing resolution must notify on the class' lock
  // when resolution has occurred. This happens in mirror::Class::SetStatus. As resolution may
  // retire a class, the version of the class in the table is returned and this may differ from
  // the class passed in.
  mirror::Class* EnsureResolved(Thread* self, const char* descriptor, mirror::Class* klass)
      WARN_UNUSED
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!dex_lock_);

  void FixupTemporaryDeclaringClass(mirror::Class* temp_class, mirror::Class* new_class)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void SetClassRoot(ClassRoot class_root, mirror::Class* klass)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Return the quick generic JNI stub for testing.
  const void* GetRuntimeQuickGenericJniStub() const;

  bool CanWeInitializeClass(mirror::Class* klass, bool can_init_statics, bool can_init_parents)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void UpdateClassMethods(mirror::Class* klass,
                          LengthPrefixedArray<ArtMethod>* new_methods)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::classlinker_classes_lock_);

  // new_class_set is the set of classes that were read from the class table section in the image.
  // If there was no class table section, it is null.
  bool UpdateAppImageClassLoadersAndDexCaches(
      gc::space::ImageSpace* space,
      Handle<mirror::ClassLoader> class_loader,
      Handle<mirror::ObjectArray<mirror::DexCache>> dex_caches,
      ClassTable::ClassSet* new_class_set,
      bool* out_forward_dex_cache_array,
      std::string* out_error_msg)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Check that c1 == FindSystemClass(self, descriptor). Abort with class dumps otherwise.
  void CheckSystemClass(Thread* self, Handle<mirror::Class> c1, const char* descriptor)
      REQUIRES(!dex_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  // Sets imt_ref appropriately for LinkInterfaceMethods.
  // If there is no method in the imt location of imt_ref it will store the given method there.
  // Otherwise it will set the conflict method which will figure out which method to use during
  // runtime.
  void SetIMTRef(ArtMethod* unimplemented_method,
                 ArtMethod* imt_conflict_method,
                 ArtMethod* current_method,
                 /*out*/bool* new_conflict,
                 /*out*/ArtMethod** imt_ref) SHARED_REQUIRES(Locks::mutator_lock_);

  void FillIMTFromIfTable(mirror::IfTable* if_table,
                          ArtMethod* unimplemented_method,
                          ArtMethod* imt_conflict_method,
                          mirror::Class* klass,
                          bool create_conflict_tables,
                          bool ignore_copied_methods,
                          /*out*/bool* new_conflict,
                          /*out*/ArtMethod** imt) SHARED_REQUIRES(Locks::mutator_lock_);

  void FillImtFromSuperClass(Handle<mirror::Class> klass,
                             ArtMethod* unimplemented_method,
                             ArtMethod* imt_conflict_method,
                             bool* new_conflict,
                             ArtMethod** imt) SHARED_REQUIRES(Locks::mutator_lock_);

  std::vector<const DexFile*> boot_class_path_;
  std::vector<std::unique_ptr<const DexFile>> boot_dex_files_;

  mutable ReaderWriterMutex dex_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  // JNI weak globals and side data to allow dex caches to get unloaded. We lazily delete weak
  // globals when we register new dex files.
  std::list<DexCacheData> dex_caches_ GUARDED_BY(dex_lock_);

  // This contains the class loaders which have class tables. It is populated by
  // InsertClassTableForClassLoader.
  std::list<ClassLoaderData> class_loaders_
      GUARDED_BY(Locks::classlinker_classes_lock_);

  // Boot class path table. Since the class loader for this is null.
  ClassTable boot_class_table_ GUARDED_BY(Locks::classlinker_classes_lock_);

  // New class roots, only used by CMS since the GC needs to mark these in the pause.
  std::vector<GcRoot<mirror::Class>> new_class_roots_ GUARDED_BY(Locks::classlinker_classes_lock_);

  // Do we need to search dex caches to find boot image classes?
  bool dex_cache_boot_image_class_lookup_required_;
  // Number of times we've searched dex caches for a class. After a certain number of misses we move
  // the classes into the class_table_ to avoid dex cache based searches.
  Atomic<uint32_t> failed_dex_cache_class_lookups_;

  // Well known mirror::Class roots.
  GcRoot<mirror::ObjectArray<mirror::Class>> class_roots_;

  // The interface table used by all arrays.
  GcRoot<mirror::IfTable> array_iftable_;

  // A cache of the last FindArrayClass results. The cache serves to avoid creating array class
  // descriptors for the sake of performing FindClass.
  static constexpr size_t kFindArrayCacheSize = 16;
  GcRoot<mirror::Class> find_array_class_cache_[kFindArrayCacheSize];
  size_t find_array_class_cache_next_victim_;

  bool init_done_;
  bool log_new_class_table_roots_ GUARDED_BY(Locks::classlinker_classes_lock_);

  InternTable* intern_table_;

  // Trampolines within the image the bounce to runtime entrypoints. Done so that there is a single
  // patch point within the image. TODO: make these proper relocations.
  const void* quick_resolution_trampoline_;
  const void* quick_imt_conflict_trampoline_;
  const void* quick_generic_jni_trampoline_;
  const void* quick_to_interpreter_bridge_trampoline_;

  // Image pointer size.
  size_t image_pointer_size_;

  class FindVirtualMethodHolderVisitor;
  friend struct CompilationHelper;  // For Compile in ImageTest.
  friend class ImageDumper;  // for DexLock
  friend class ImageWriter;  // for GetClassRoots
  friend class JniCompilerTest;  // for GetRuntimeQuickGenericJniStub
  friend class JniInternalTest;  // for GetRuntimeQuickGenericJniStub
  ART_FRIEND_TEST(ClassLinkerTest, RegisterDexFileName);  // for DexLock, and RegisterDexFileLocked
  ART_FRIEND_TEST(mirror::DexCacheTest, Open);  // for AllocDexCache
  DISALLOW_COPY_AND_ASSIGN(ClassLinker);
};

}  // namespace art

#endif  // ART_RUNTIME_CLASS_LINKER_H_
