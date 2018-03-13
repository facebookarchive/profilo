/*
 * Copyright 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_JIT_H_
#define ART_RUNTIME_JIT_JIT_H_

#include <unordered_map>

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/timing_logger.h"
#include "gc_root.h"
#include "jni.h"
#include "object_callbacks.h"
#include "thread_pool.h"

namespace art {

class ArtMethod;
class CompilerCallbacks;
struct RuntimeArgumentMap;

namespace jit {

class JitCodeCache;
class JitInstrumentationCache;
class JitOptions;

class Jit {
 public:
  static constexpr bool kStressMode = kIsDebugBuild;
  static constexpr size_t kDefaultCompileThreshold = kStressMode ? 1 : 1000;

  virtual ~Jit();
  static Jit* Create(JitOptions* options, std::string* error_msg);
  bool CompileMethod(ArtMethod* method, Thread* self)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void CreateInstrumentationCache(size_t compile_threshold);
  void CreateThreadPool();
  CompilerCallbacks* GetCompilerCallbacks() {
    return compiler_callbacks_;
  }
  const JitCodeCache* GetCodeCache() const {
    return code_cache_.get();
  }
  JitCodeCache* GetCodeCache() {
    return code_cache_.get();
  }
  void DeleteThreadPool();
  // Dump interesting info: #methods compiled, code vs data size, compile / verify cumulative
  // loggers.
  void DumpInfo(std::ostream& os);
  // Add a timing logger to cumulative_timings_.
  void AddTimingLogger(const TimingLogger& logger);

 private:
  Jit();
  bool LoadCompiler(std::string* error_msg);

  // JIT compiler
  void* jit_library_handle_;
  void* jit_compiler_handle_;
  void* (*jit_load_)(CompilerCallbacks**);
  void (*jit_unload_)(void*);
  bool (*jit_compile_method_)(void*, ArtMethod*, Thread*);

  // Performance monitoring.
  bool dump_info_on_shutdown_;
  CumulativeLogger cumulative_timings_;

  std::unique_ptr<jit::JitInstrumentationCache> instrumentation_cache_;
  std::unique_ptr<jit::JitCodeCache> code_cache_;
  CompilerCallbacks* compiler_callbacks_;  // Owned by the jit compiler.

  DISALLOW_COPY_AND_ASSIGN(Jit);
};

class JitOptions {
 public:
  static JitOptions* CreateFromRuntimeArguments(const RuntimeArgumentMap& options);
  size_t GetCompileThreshold() const {
    return compile_threshold_;
  }
  size_t GetCodeCacheCapacity() const {
    return code_cache_capacity_;
  }
  bool DumpJitInfoOnShutdown() const {
    return dump_info_on_shutdown_;
  }
  bool UseJIT() const {
    return use_jit_;
  }
  void SetUseJIT(bool b) {
    use_jit_ = b;
  }

 private:
  bool use_jit_;
  size_t code_cache_capacity_;
  size_t compile_threshold_;
  bool dump_info_on_shutdown_;

  JitOptions() : use_jit_(false), code_cache_capacity_(0), compile_threshold_(0),
      dump_info_on_shutdown_(false) { }

  DISALLOW_COPY_AND_ASSIGN(JitOptions);
};

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_H_
