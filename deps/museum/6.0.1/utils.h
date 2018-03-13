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

#ifndef ART_RUNTIME_UTILS_H_
#define ART_RUNTIME_UTILS_H_

#include <pthread.h>

#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "arch/instruction_set.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "globals.h"
#include "primitive.h"

namespace art {

class ArtField;
class ArtMethod;
class DexFile;

namespace mirror {
class Class;
class Object;
class String;
}  // namespace mirror

template <typename T>
bool ParseUint(const char *in, T* out) {
  char* end;
  unsigned long long int result = strtoull(in, &end, 0);  // NOLINT(runtime/int)
  if (in == end || *end != '\0') {
    return false;
  }
  if (std::numeric_limits<T>::max() < result) {
    return false;
  }
  *out = static_cast<T>(result);
  return true;
}

template <typename T>
bool ParseInt(const char* in, T* out) {
  char* end;
  long long int result = strtoll(in, &end, 0);  // NOLINT(runtime/int)
  if (in == end || *end != '\0') {
    return false;
  }
  if (result < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < result) {
    return false;
  }
  *out = static_cast<T>(result);
  return true;
}

// Return whether x / divisor == x * (1.0f / divisor), for every float x.
static constexpr bool CanDivideByReciprocalMultiplyFloat(int32_t divisor) {
  // True, if the most significant bits of divisor are 0.
  return ((divisor & 0x7fffff) == 0);
}

// Return whether x / divisor == x * (1.0 / divisor), for every double x.
static constexpr bool CanDivideByReciprocalMultiplyDouble(int64_t divisor) {
  // True, if the most significant bits of divisor are 0.
  return ((divisor & ((UINT64_C(1) << 52) - 1)) == 0);
}

static inline uint32_t PointerToLowMemUInt32(const void* p) {
  uintptr_t intp = reinterpret_cast<uintptr_t>(p);
  DCHECK_LE(intp, 0xFFFFFFFFU);
  return intp & 0xFFFFFFFFU;
}

static inline bool NeedsEscaping(uint16_t ch) {
  return (ch < ' ' || ch > '~');
}

std::string PrintableChar(uint16_t ch);

// Returns an ASCII string corresponding to the given UTF-8 string.
// Java escapes are used for non-ASCII characters.
std::string PrintableString(const char* utf8);

// Tests whether 's' starts with 'prefix'.
bool StartsWith(const std::string& s, const char* prefix);

// Tests whether 's' ends with 'suffix'.
bool EndsWith(const std::string& s, const char* suffix);

// Used to implement PrettyClass, PrettyField, PrettyMethod, and PrettyTypeOf,
// one of which is probably more useful to you.
// Returns a human-readable equivalent of 'descriptor'. So "I" would be "int",
// "[[I" would be "int[][]", "[Ljava/lang/String;" would be
// "java.lang.String[]", and so forth.
std::string PrettyDescriptor(mirror::String* descriptor)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
std::string PrettyDescriptor(const char* descriptor);
std::string PrettyDescriptor(mirror::Class* klass)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
std::string PrettyDescriptor(Primitive::Type type);

// Returns a human-readable signature for 'f'. Something like "a.b.C.f" or
// "int a.b.C.f" (depending on the value of 'with_type').
std::string PrettyField(ArtField* f, bool with_type = true)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
std::string PrettyField(uint32_t field_idx, const DexFile& dex_file, bool with_type = true);

// Returns a human-readable signature for 'm'. Something like "a.b.C.m" or
// "a.b.C.m(II)V" (depending on the value of 'with_signature').
std::string PrettyMethod(ArtMethod* m, bool with_signature = true)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
std::string PrettyMethod(uint32_t method_idx, const DexFile& dex_file, bool with_signature = true);

// Returns a human-readable form of the name of the *class* of the given object.
// So given an instance of java.lang.String, the output would
// be "java.lang.String". Given an array of int, the output would be "int[]".
// Given String.class, the output would be "java.lang.Class<java.lang.String>".
std::string PrettyTypeOf(mirror::Object* obj)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Returns a human-readable form of the type at an index in the specified dex file.
// Example outputs: char[], java.lang.String.
std::string PrettyType(uint32_t type_idx, const DexFile& dex_file);

// Returns a human-readable form of the name of the given class.
// Given String.class, the output would be "java.lang.Class<java.lang.String>".
std::string PrettyClass(mirror::Class* c)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Returns a human-readable form of the name of the given class with its class loader.
std::string PrettyClassAndClassLoader(mirror::Class* c)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

// Returns a human-readable version of the Java part of the access flags, e.g., "private static "
// (note the trailing whitespace).
std::string PrettyJavaAccessFlags(uint32_t access_flags);

// Returns a human-readable size string such as "1MB".
std::string PrettySize(int64_t size_in_bytes);

// Performs JNI name mangling as described in section 11.3 "Linking Native Methods"
// of the JNI spec.
std::string MangleForJni(const std::string& s);

// Turn "java.lang.String" into "Ljava/lang/String;".
std::string DotToDescriptor(const char* class_name);

// Turn "Ljava/lang/String;" into "java.lang.String" using the conventions of
// java.lang.Class.getName().
std::string DescriptorToDot(const char* descriptor);

// Turn "Ljava/lang/String;" into "java/lang/String" using the opposite conventions of
// java.lang.Class.getName().
std::string DescriptorToName(const char* descriptor);

// Tests for whether 's' is a valid class name in the three common forms:
bool IsValidBinaryClassName(const char* s);  // "java.lang.String"
bool IsValidJniClassName(const char* s);     // "java/lang/String"
bool IsValidDescriptor(const char* s);       // "Ljava/lang/String;"

// Returns whether the given string is a valid field or method name,
// additionally allowing names that begin with '<' and end with '>'.
bool IsValidMemberName(const char* s);

// Returns the JNI native function name for the non-overloaded method 'm'.
std::string JniShortName(ArtMethod* m)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
// Returns the JNI native function name for the overloaded method 'm'.
std::string JniLongName(ArtMethod* m)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

bool ReadFileToString(const std::string& file_name, std::string* result);
bool PrintFileToLog(const std::string& file_name, LogSeverity level);

// Splits a string using the given separator character into a vector of
// strings. Empty strings will be omitted.
void Split(const std::string& s, char separator, std::vector<std::string>* result);

// Trims whitespace off both ends of the given string.
std::string Trim(const std::string& s);

// Joins a vector of strings into a single string, using the given separator.
template <typename StringT> std::string Join(const std::vector<StringT>& strings, char separator);

// Returns the calling thread's tid. (The C libraries don't expose this.)
pid_t GetTid();

// Returns the given thread's name.
std::string GetThreadName(pid_t tid);

// Returns details of the given thread's stack.
void GetThreadStack(pthread_t thread, void** stack_base, size_t* stack_size, size_t* guard_size);

// Reads data from "/proc/self/task/${tid}/stat".
void GetTaskStats(pid_t tid, char* state, int* utime, int* stime, int* task_cpu);

// Returns the name of the scheduler group for the given thread the current process, or the empty string.
std::string GetSchedulerGroupName(pid_t tid);

// Sets the name of the current thread. The name may be truncated to an
// implementation-defined limit.
void SetThreadName(const char* thread_name);

// Dumps the native stack for thread 'tid' to 'os'.
void DumpNativeStack(std::ostream& os, pid_t tid, const char* prefix = "",
    ArtMethod* current_method = nullptr, void* ucontext = nullptr)
    NO_THREAD_SAFETY_ANALYSIS;

// Dumps the kernel stack for thread 'tid' to 'os'. Note that this is only available on linux-x86.
void DumpKernelStack(std::ostream& os, pid_t tid, const char* prefix = "", bool include_count = true);

// Find $ANDROID_ROOT, /system, or abort.
const char* GetAndroidRoot();

// Find $ANDROID_DATA, /data, or abort.
const char* GetAndroidData();
// Find $ANDROID_DATA, /data, or return null.
const char* GetAndroidDataSafe(std::string* error_msg);

// Returns the dalvik-cache location, with subdir appended. Returns the empty string if the cache
// could not be found (or created).
std::string GetDalvikCache(const char* subdir, bool create_if_absent = true);
// Returns the dalvik-cache location, or dies trying. subdir will be
// appended to the cache location.
std::string GetDalvikCacheOrDie(const char* subdir, bool create_if_absent = true);
// Return true if we found the dalvik cache and stored it in the dalvik_cache argument.
// have_android_data will be set to true if we have an ANDROID_DATA that exists,
// dalvik_cache_exists will be true if there is a dalvik-cache directory that is present.
// The flag is_global_cache tells whether this cache is /data/dalvik-cache.
void GetDalvikCache(const char* subdir, bool create_if_absent, std::string* dalvik_cache,
                    bool* have_android_data, bool* dalvik_cache_exists, bool* is_global_cache);

// Returns the absolute dalvik-cache path for a DexFile or OatFile. The path returned will be
// rooted at cache_location.
bool GetDalvikCacheFilename(const char* file_location, const char* cache_location,
                            std::string* filename, std::string* error_msg);
// Returns the absolute dalvik-cache path for a DexFile or OatFile, or
// dies trying. The path returned will be rooted at cache_location.
std::string GetDalvikCacheFilenameOrDie(const char* file_location,
                                        const char* cache_location);

// Returns the system location for an image
std::string GetSystemImageFilename(const char* location, InstructionSet isa);

// Check whether the given magic matches a known file type.
bool IsZipMagic(uint32_t magic);
bool IsDexMagic(uint32_t magic);
bool IsOatMagic(uint32_t magic);

// Wrapper on fork/execv to run a command in a subprocess.
bool Exec(std::vector<std::string>& arg_vector, std::string* error_msg);

class VoidFunctor {
 public:
  template <typename A>
  inline void operator() (A a) const {
    UNUSED(a);
  }

  template <typename A, typename B>
  inline void operator() (A a, B b) const {
    UNUSED(a, b);
  }

  template <typename A, typename B, typename C>
  inline void operator() (A a, B b, C c) const {
    UNUSED(a, b, c);
  }
};

template <typename Alloc>
void Push32(std::vector<uint8_t, Alloc>* buf, int32_t data) {
  buf->push_back(data & 0xff);
  buf->push_back((data >> 8) & 0xff);
  buf->push_back((data >> 16) & 0xff);
  buf->push_back((data >> 24) & 0xff);
}

void EncodeUnsignedLeb128(uint32_t data, std::vector<uint8_t>* buf);
void EncodeSignedLeb128(int32_t data, std::vector<uint8_t>* buf);

// Deleter using free() for use with std::unique_ptr<>. See also UniqueCPtr<> below.
struct FreeDelete {
  // NOTE: Deleting a const object is valid but free() takes a non-const pointer.
  void operator()(const void* ptr) const {
    free(const_cast<void*>(ptr));
  }
};

// Alias for std::unique_ptr<> that uses the C function free() to delete objects.
template <typename T>
using UniqueCPtr = std::unique_ptr<T, FreeDelete>;

// C++14 from-the-future import (std::make_unique)
// Invoke the constructor of 'T' with the provided args, and wrap the result in a unique ptr.
template <typename T, typename ... Args>
std::unique_ptr<T> MakeUnique(Args&& ... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

inline bool TestBitmap(size_t idx, const uint8_t* bitmap) {
  return ((bitmap[idx / kBitsPerByte] >> (idx % kBitsPerByte)) & 0x01) != 0;
}

static inline constexpr bool ValidPointerSize(size_t pointer_size) {
  return pointer_size == 4 || pointer_size == 8;
}

}  // namespace art

#endif  // ART_RUNTIME_UTILS_H_
