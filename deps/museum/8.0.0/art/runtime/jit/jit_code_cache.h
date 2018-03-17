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

#ifndef ART_RUNTIME_JIT_JIT_CODE_CACHE_H_
#define ART_RUNTIME_JIT_JIT_CODE_CACHE_H_

#include "instrumentation.h"

#include "atomic.h"
#include "base/arena_containers.h"
#include "base/histogram-inl.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc/accounting/bitmap.h"
#include "gc_root.h"
#include "jni.h"
#include "method_reference.h"
#include "oat_file.h"
#include "object_callbacks.h"
#include "profile_compilation_info.h"
#include "safe_map.h"
#include "thread_pool.h"

namespace art {

class ArtMethod;
class LinearAlloc;
class InlineCache;
class ProfilingInfo;

namespace jit {

class JitInstrumentationCache;

// Alignment in bits that will suit all architectures.
static constexpr int kJitCodeAlignment = 16;
using CodeCacheBitmap = gc::accounting::MemoryRangeBitmap<kJitCodeAlignment>;

class JitCodeCache {
 public:
  static constexpr size_t kMaxCapacity = 64 * MB;
  // Put the default to a very low amount for debug builds to stress the code cache
  // collection.
  static constexpr size_t kInitialCapacity = kIsDebugBuild ? 8 * KB : 64 * KB;

  // By default, do not GC until reaching 256KB.
  static constexpr size_t kReservedCapacity = kInitialCapacity * 4;

  // Create the code cache with a code + data capacity equal to "capacity", error message is passed
  // in the out arg error_msg.
  static JitCodeCache* Create(size_t initial_capacity,
                              size_t max_capacity,
                              bool generate_debug_info,
                              std::string* error_msg);

  // Number of bytes allocated in the code cache.
  size_t CodeCacheSize() REQUIRES(!lock_);

  // Number of bytes allocated in the data cache.
  size_t DataCacheSize() REQUIRES(!lock_);

  bool NotifyCompilationOf(ArtMethod* method, Thread* self, bool osr)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void NotifyMethodRedefined(ArtMethod* method)
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  // Notify to the code cache that the compiler wants to use the
  // profiling info of `method` to drive optimizations,
  // and therefore ensure the returned profiling info object is not
  // collected.
  ProfilingInfo* NotifyCompilerUse(ArtMethod* method, Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void DoneCompiling(ArtMethod* method, Thread* self, bool osr)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void DoneCompilerUse(ArtMethod* method, Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  // Allocate and write code and its metadata to the code cache.
  // `cha_single_implementation_list` needs to be registered via CHA (if it's
  // still valid), since the compiled code still needs to be invalidated if the
  // single-implementation assumptions are violated later. This needs to be done
  // even if `has_should_deoptimize_flag` is false, which can happen due to CHA
  // guard elimination.
  uint8_t* CommitCode(Thread* self,
                      ArtMethod* method,
                      uint8_t* stack_map,
                      uint8_t* method_info,
                      uint8_t* roots_data,
                      size_t frame_size_in_bytes,
                      size_t core_spill_mask,
                      size_t fp_spill_mask,
                      const uint8_t* code,
                      size_t code_size,
                      size_t data_size,
                      bool osr,
                      Handle<mirror::ObjectArray<mirror::Object>> roots,
                      bool has_should_deoptimize_flag,
                      const ArenaSet<ArtMethod*>& cha_single_implementation_list)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  // Return true if the code cache contains this pc.
  bool ContainsPc(const void* pc) const;

  // Return true if the code cache contains this method.
  bool ContainsMethod(ArtMethod* method) REQUIRES(!lock_);

  // Allocate a region of data that contain `size` bytes, and potentially space
  // for storing `number_of_roots` roots. Returns null if there is no more room.
  // Return the number of bytes allocated.
  size_t ReserveData(Thread* self,
                     size_t stack_map_size,
                     size_t method_info_size,
                     size_t number_of_roots,
                     ArtMethod* method,
                     uint8_t** stack_map_data,
                     uint8_t** method_info_data,
                     uint8_t** roots_data)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  // Clear data from the data portion of the code cache.
  void ClearData(Thread* self, uint8_t* stack_map_data, uint8_t* roots_data)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  CodeCacheBitmap* GetLiveBitmap() const {
    return live_bitmap_.get();
  }

  // Return whether we should do a full collection given the current state of the cache.
  bool ShouldDoFullCollection()
      REQUIRES(lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Perform a collection on the code cache.
  void GarbageCollectCache(Thread* self)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given the 'pc', try to find the JIT compiled code associated with it.
  // Return null if 'pc' is not in the code cache. 'method' is passed for
  // sanity check.
  OatQuickMethodHeader* LookupMethodHeader(uintptr_t pc, ArtMethod* method)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  OatQuickMethodHeader* LookupOsrMethodHeader(ArtMethod* method)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Remove all methods in our cache that were allocated by 'alloc'.
  void RemoveMethodsIn(Thread* self, const LinearAlloc& alloc)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void CopyInlineCacheInto(const InlineCache& ic, Handle<mirror::ObjectArray<mirror::Class>> array)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Create a 'ProfileInfo' for 'method'. If 'retry_allocation' is true,
  // will collect and retry if the first allocation is unsuccessful.
  ProfilingInfo* AddProfilingInfo(Thread* self,
                                  ArtMethod* method,
                                  const std::vector<uint32_t>& entries,
                                  bool retry_allocation)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool OwnsSpace(const void* mspace) const NO_THREAD_SAFETY_ANALYSIS {
    return mspace == code_mspace_ || mspace == data_mspace_;
  }

  void* MoreCore(const void* mspace, intptr_t increment);

  // Adds to `methods` all profiled methods which are part of any of the given dex locations.
  void GetProfiledMethods(const std::set<std::string>& dex_base_locations,
                          std::vector<ProfileMethodInfo>& methods)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  uint64_t GetLastUpdateTimeNs() const;

  size_t GetCurrentCapacity() REQUIRES(!lock_) {
    MutexLock lock(Thread::Current(), lock_);
    return current_capacity_;
  }

  size_t GetMemorySizeOfCodePointer(const void* ptr) REQUIRES(!lock_);

  void InvalidateCompiledCodeFor(ArtMethod* method, const OatQuickMethodHeader* code)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void Dump(std::ostream& os) REQUIRES(!lock_);

  bool IsOsrCompiled(ArtMethod* method) REQUIRES(!lock_);

  void SweepRootTables(IsMarkedVisitor* visitor)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // The GC needs to disallow the reading of inline caches when it processes them,
  // to avoid having a class being used while it is being deleted.
  void AllowInlineCacheAccess() REQUIRES(!lock_);
  void DisallowInlineCacheAccess() REQUIRES(!lock_);
  void BroadcastForInlineCacheAccess() REQUIRES(!lock_);

  // Notify the code cache that the method at the pointer 'old_method' is being moved to the pointer
  // 'new_method' since it is being made obsolete.
  void MoveObsoleteMethod(ArtMethod* old_method, ArtMethod* new_method)
      REQUIRES(!lock_) REQUIRES(Locks::mutator_lock_);

  // Dynamically change whether we want to garbage collect code. Should only be used
  // by tests.
  void SetGarbageCollectCode(bool value) {
    garbage_collect_code_ = value;
  }

 private:
  // Take ownership of maps.
  JitCodeCache(MemMap* code_map,
               MemMap* data_map,
               size_t initial_code_capacity,
               size_t initial_data_capacity,
               size_t max_capacity,
               bool garbage_collect_code);

  // Internal version of 'CommitCode' that will not retry if the
  // allocation fails. Return null if the allocation fails.
  uint8_t* CommitCodeInternal(Thread* self,
                              ArtMethod* method,
                              uint8_t* stack_map,
                              uint8_t* method_info,
                              uint8_t* roots_data,
                              size_t frame_size_in_bytes,
                              size_t core_spill_mask,
                              size_t fp_spill_mask,
                              const uint8_t* code,
                              size_t code_size,
                              size_t data_size,
                              bool osr,
                              Handle<mirror::ObjectArray<mirror::Object>> roots,
                              bool has_should_deoptimize_flag,
                              const ArenaSet<ArtMethod*>& cha_single_implementation_list)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ProfilingInfo* AddProfilingInfoInternal(Thread* self,
                                          ArtMethod* method,
                                          const std::vector<uint32_t>& entries)
      REQUIRES(lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // If a collection is in progress, wait for it to finish. Return
  // whether the thread actually waited.
  bool WaitForPotentialCollectionToComplete(Thread* self)
      REQUIRES(lock_) REQUIRES(!Locks::mutator_lock_);

  // Remove CHA dependents and underlying allocations for entries in `method_headers`.
  void FreeAllMethodHeaders(const std::unordered_set<OatQuickMethodHeader*>& method_headers)
      REQUIRES(!lock_)
      REQUIRES(!Locks::cha_lock_);

  // Free in the mspace allocations for `code_ptr`.
  void FreeCode(const void* code_ptr) REQUIRES(lock_);

  // Number of bytes allocated in the code cache.
  size_t CodeCacheSizeLocked() REQUIRES(lock_);

  // Number of bytes allocated in the data cache.
  size_t DataCacheSizeLocked() REQUIRES(lock_);

  // Notify all waiting threads that a collection is done.
  void NotifyCollectionDone(Thread* self) REQUIRES(lock_);

  // Try to increase the current capacity of the code cache. Return whether we
  // succeeded at doing so.
  bool IncreaseCodeCacheCapacity() REQUIRES(lock_);

  // Set the footprint limit of the code cache.
  void SetFootprintLimit(size_t new_footprint) REQUIRES(lock_);

  void DoCollection(Thread* self, bool collect_profiling_info)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void RemoveUnmarkedCode(Thread* self)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void MarkCompiledCodeOnThreadStacks(Thread* self)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool CheckLiveCompiledCodeHasProfilingInfo()
      REQUIRES(lock_);

  void FreeCode(uint8_t* code) REQUIRES(lock_);
  uint8_t* AllocateCode(size_t code_size) REQUIRES(lock_);
  void FreeData(uint8_t* data) REQUIRES(lock_);
  uint8_t* AllocateData(size_t data_size) REQUIRES(lock_);

  bool IsWeakAccessEnabled(Thread* self) const;
  void WaitUntilInlineCacheAccessible(Thread* self)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Lock for guarding allocations, collections, and the method_code_map_.
  Mutex lock_;
  // Condition to wait on during collection.
  ConditionVariable lock_cond_ GUARDED_BY(lock_);
  // Whether there is a code cache collection in progress.
  bool collection_in_progress_ GUARDED_BY(lock_);
  // Mem map which holds code.
  std::unique_ptr<MemMap> code_map_;
  // Mem map which holds data (stack maps and profiling info).
  std::unique_ptr<MemMap> data_map_;
  // The opaque mspace for allocating code.
  void* code_mspace_ GUARDED_BY(lock_);
  // The opaque mspace for allocating data.
  void* data_mspace_ GUARDED_BY(lock_);
  // Bitmap for collecting code and data.
  std::unique_ptr<CodeCacheBitmap> live_bitmap_;
  // Holds compiled code associated to the ArtMethod.
  SafeMap<const void*, ArtMethod*> method_code_map_ GUARDED_BY(lock_);
  // Holds osr compiled code associated to the ArtMethod.
  SafeMap<ArtMethod*, const void*> osr_code_map_ GUARDED_BY(lock_);
  // ProfilingInfo objects we have allocated.
  std::vector<ProfilingInfo*> profiling_infos_ GUARDED_BY(lock_);

  // The maximum capacity in bytes this code cache can go to.
  size_t max_capacity_ GUARDED_BY(lock_);

  // The current capacity in bytes of the code cache.
  size_t current_capacity_ GUARDED_BY(lock_);

  // The current footprint in bytes of the code portion of the code cache.
  size_t code_end_ GUARDED_BY(lock_);

  // The current footprint in bytes of the data portion of the code cache.
  size_t data_end_ GUARDED_BY(lock_);

  // Whether the last collection round increased the code cache.
  bool last_collection_increased_code_cache_ GUARDED_BY(lock_);

  // Last time the the code_cache was updated.
  // It is atomic to avoid locking when reading it.
  Atomic<uint64_t> last_update_time_ns_;

  // Whether we can do garbage collection. Not 'const' as tests may override this.
  bool garbage_collect_code_;

  // The size in bytes of used memory for the data portion of the code cache.
  size_t used_memory_for_data_ GUARDED_BY(lock_);

  // The size in bytes of used memory for the code portion of the code cache.
  size_t used_memory_for_code_ GUARDED_BY(lock_);

  // Number of compilations done throughout the lifetime of the JIT.
  size_t number_of_compilations_ GUARDED_BY(lock_);

  // Number of compilations for on-stack-replacement done throughout the lifetime of the JIT.
  size_t number_of_osr_compilations_ GUARDED_BY(lock_);

  // Number of code cache collections done throughout the lifetime of the JIT.
  size_t number_of_collections_ GUARDED_BY(lock_);

  // Histograms for keeping track of stack map size statistics.
  Histogram<uint64_t> histogram_stack_map_memory_use_ GUARDED_BY(lock_);

  // Histograms for keeping track of code size statistics.
  Histogram<uint64_t> histogram_code_memory_use_ GUARDED_BY(lock_);

  // Histograms for keeping track of profiling info statistics.
  Histogram<uint64_t> histogram_profiling_info_memory_use_ GUARDED_BY(lock_);

  // Whether the GC allows accessing weaks in inline caches. Note that this
  // is not used by the concurrent collector, which uses
  // Thread::SetWeakRefAccessEnabled instead.
  Atomic<bool> is_weak_access_enabled_;

  // Condition to wait on for accessing inline caches.
  ConditionVariable inline_cache_cond_ GUARDED_BY(lock_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JitCodeCache);
};

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_CODE_CACHE_H_
