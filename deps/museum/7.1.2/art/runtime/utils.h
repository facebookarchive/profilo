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
#include <stdlib.h>

#include <limits>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "arch/instruction_set.h"
#include "base/casts.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/stringpiece.h"
#include "globals.h"
#include "primitive.h"

class BacktraceMap;

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

template <typename T> T SafeAbs(T value) {
  // std::abs has undefined behavior on min limits.
  DCHECK_NE(value, std::numeric_limits<T>::min());
  return std::abs(value);
}

template <typename T> T AbsOrMin(T value) {
  return (value == std::numeric_limits<T>::min())
      ? value
      : std::abs(value);
}

template <typename T>
inline typename std::make_unsigned<T>::type MakeUnsigned(T x) {
  return static_cast<typename std::make_unsigned<T>::type>(x);
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
    SHARED_REQUIRES(Locks::mutator_lock_);
std::string PrettyDescriptor(const char* descriptor);
std::string PrettyDescriptor(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_);
std::string PrettyDescriptor(Primitive::Type type);

// Returns a human-readable signature for 'f'. Something like "a.b.C.f" or
// "int a.b.C.f" (depending on the value of 'with_type').
std::string PrettyField(ArtField* f, bool with_type = true)
    SHARED_REQUIRES(Locks::mutator_lock_);
std::string PrettyField(uint32_t field_idx, const DexFile& dex_file, bool with_type = true);

// Returns a human-readable signature for 'm'. Something like "a.b.C.m" or
// "a.b.C.m(II)V" (depending on the value of 'with_signature').
std::string PrettyMethod(ArtMethod* m, bool with_signature = true)
    SHARED_REQUIRES(Locks::mutator_lock_);
std::string PrettyMethod(uint32_t method_idx, const DexFile& dex_file, bool with_signature = true);

// Returns a human-readable form of the name of the *class* of the given object.
// So given an instance of java.lang.String, the output would
// be "java.lang.String". Given an array of int, the output would be "int[]".
// Given String.class, the output would be "java.lang.Class<java.lang.String>".
std::string PrettyTypeOf(mirror::Object* obj)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Returns a human-readable form of the type at an index in the specified dex file.
// Example outputs: char[], java.lang.String.
std::string PrettyType(uint32_t type_idx, const DexFile& dex_file);

// Returns a human-readable form of the name of the given class.
// Given String.class, the output would be "java.lang.Class<java.lang.String>".
std::string PrettyClass(mirror::Class* c)
    SHARED_REQUIRES(Locks::mutator_lock_);

// Returns a human-readable form of the name of the given class with its class loader.
std::string PrettyClassAndClassLoader(mirror::Class* c)
    SHARED_REQUIRES(Locks::mutator_lock_);

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
    SHARED_REQUIRES(Locks::mutator_lock_);
// Returns the JNI native function name for the overloaded method 'm'.
std::string JniLongName(ArtMethod* m)
    SHARED_REQUIRES(Locks::mutator_lock_);

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
void DumpNativeStack(std::ostream& os,
                     pid_t tid,
                     BacktraceMap* map = nullptr,
                     const char* prefix = "",
                     ArtMethod* current_method = nullptr,
                     void* ucontext = nullptr)
    NO_THREAD_SAFETY_ANALYSIS;

// Dumps the kernel stack for thread 'tid' to 'os'. Note that this is only available on linux-x86.
void DumpKernelStack(std::ostream& os,
                     pid_t tid,
                     const char* prefix = "",
                     bool include_count = true);

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

// Wrapper on fork/execv to run a command in a subprocess.
// Both of these spawn child processes using the environment as it was set when the single instance
// of the runtime (Runtime::Current()) was started.  If no instance of the runtime was started, it
// will use the current environment settings.
bool Exec(std::vector<std::string>& arg_vector, std::string* error_msg);
int ExecAndReturnCode(std::vector<std::string>& arg_vector, std::string* error_msg);

// Returns true if the file exists.
bool FileExists(const std::string& filename);
bool FileExistsAndNotEmpty(const std::string& filename);

class VoidFunctor {
 public:
  template <typename A>
  inline void operator() (A a ATTRIBUTE_UNUSED) const {
  }

  template <typename A, typename B>
  inline void operator() (A a ATTRIBUTE_UNUSED, B b ATTRIBUTE_UNUSED) const {
  }

  template <typename A, typename B, typename C>
  inline void operator() (A a ATTRIBUTE_UNUSED, B b ATTRIBUTE_UNUSED, C c ATTRIBUTE_UNUSED) const {
  }
};

template <typename Vector>
void Push32(Vector* buf, int32_t data) {
  static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");
  buf->push_back(data & 0xff);
  buf->push_back((data >> 8) & 0xff);
  buf->push_back((data >> 16) & 0xff);
  buf->push_back((data >> 24) & 0xff);
}

inline bool TestBitmap(size_t idx, const uint8_t* bitmap) {
  return ((bitmap[idx / kBitsPerByte] >> (idx % kBitsPerByte)) & 0x01) != 0;
}

static inline constexpr bool ValidPointerSize(size_t pointer_size) {
  return pointer_size == 4 || pointer_size == 8;
}

void DumpMethodCFG(ArtMethod* method, std::ostream& os) SHARED_REQUIRES(Locks::mutator_lock_);
void DumpMethodCFG(const DexFile* dex_file, uint32_t dex_method_idx, std::ostream& os);

static inline const void* EntryPointToCodePointer(const void* entry_point) {
  uintptr_t code = reinterpret_cast<uintptr_t>(entry_point);
  // TODO: Make this Thumb2 specific. It is benign on other architectures as code is always at
  //       least 2 byte aligned.
  code &= ~0x1;
  return reinterpret_cast<const void*>(code);
}

using UsageFn = void (*)(const char*, ...);

template <typename T>
static void ParseUintOption(const StringPiece& option,
                            const std::string& option_name,
                            T* out,
                            UsageFn Usage,
                            bool is_long_option = true) {
  std::string option_prefix = option_name + (is_long_option ? "=" : "");
  DCHECK(option.starts_with(option_prefix)) << option << " " << option_prefix;
  const char* value_string = option.substr(option_prefix.size()).data();
  int64_t parsed_integer_value = 0;
  if (!ParseInt(value_string, &parsed_integer_value)) {
    Usage("Failed to parse %s '%s' as an integer", option_name.c_str(), value_string);
  }
  if (parsed_integer_value < 0) {
    Usage("%s passed a negative value %d", option_name.c_str(), parsed_integer_value);
  }
  *out = dchecked_integral_cast<T>(parsed_integer_value);
}

void ParseDouble(const std::string& option,
                 char after_char,
                 double min,
                 double max,
                 double* parsed_value,
                 UsageFn Usage);

#if defined(__BIONIC__)
struct Arc4RandomGenerator {
  typedef uint32_t result_type;
  static constexpr uint32_t min() { return std::numeric_limits<uint32_t>::min(); }
  static constexpr uint32_t max() { return std::numeric_limits<uint32_t>::max(); }
  uint32_t operator() () { return arc4random(); }
};
using RNG = Arc4RandomGenerator;
#else
using RNG = std::random_device;
#endif

template <typename T>
T GetRandomNumber(T min, T max) {
  CHECK_LT(min, max);
  std::uniform_int_distribution<T> dist(min, max);
  RNG rng;
  return dist(rng);
}

// Return the file size in bytes or -1 if the file does not exists.
int64_t GetFileSizeBytes(const std::string& filename);

// Sleep forever and never come back.
NO_RETURN void SleepForever();

inline void FlushInstructionCache(char* begin, char* end) {
  // Only use __builtin___clear_cache with Clang or with GCC >= 4.3.0
  // (__builtin___clear_cache was introduced in GCC 4.3.0).
#if defined(__clang__) || GCC_VERSION >= 40300
  __builtin___clear_cache(begin, end);
#else
  // Only warn on non-Intel platforms, as x86 and x86-64 do not need
  // cache flush instructions, as long as the "code uses the same
  // linear address for modifying and fetching the instruction". See
  // "Intel(R) 64 and IA-32 Architectures Software Developer's Manual
  // Volume 3A: System Programming Guide, Part 1", section 11.6
  // "Self-Modifying Code".
#if !defined(__i386__) && !defined(__x86_64__)
  UNIMPLEMENTED(WARNING) << "cache flush";
#endif
#endif
}

}  // namespace art

#endif  // ART_RUNTIME_UTILS_H_
