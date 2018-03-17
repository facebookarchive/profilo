/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_PROFILE_SAVER_H_
#define ART_RUNTIME_JIT_PROFILE_SAVER_H_

#include "base/mutex.h"
#include "jit_code_cache.h"
#include "method_reference.h"
#include "profile_compilation_info.h"
#include "profile_saver_options.h"
#include "safe_map.h"

namespace art {

class ProfileSaver {
 public:
  // Starts the profile saver thread if not already started.
  // If the saver is already running it adds (output_filename, code_paths) to its tracked locations.
  static void Start(const ProfileSaverOptions& options,
                    const std::string& output_filename,
                    jit::JitCodeCache* jit_code_cache,
                    const std::vector<std::string>& code_paths)
      REQUIRES(!Locks::profiler_lock_, !wait_lock_);

  // Stops the profile saver thread.
  // NO_THREAD_SAFETY_ANALYSIS for static function calling into member function with excludes lock.
  static void Stop(bool dump_info_)
      REQUIRES(!Locks::profiler_lock_, !wait_lock_)
      NO_THREAD_SAFETY_ANALYSIS;

  // Returns true if the profile saver is started.
  static bool IsStarted() REQUIRES(!Locks::profiler_lock_);

  // If the profile saver is running, dumps statistics to the `os`. Otherwise it does nothing.
  static void DumpInstanceInfo(std::ostream& os);

  // NO_THREAD_SAFETY_ANALYSIS for static function calling into member function with excludes lock.
  static void NotifyJitActivity()
      REQUIRES(!Locks::profiler_lock_, !wait_lock_)
      NO_THREAD_SAFETY_ANALYSIS;

  // For testing or manual purposes (SIGUSR1).
  static void ForceProcessProfiles();

  // Just for testing purposes.
  static bool HasSeenMethod(const std::string& profile, bool hot, MethodReference ref);

 private:
  ProfileSaver(const ProfileSaverOptions& options,
               const std::string& output_filename,
               jit::JitCodeCache* jit_code_cache,
               const std::vector<std::string>& code_paths);
  ~ProfileSaver();

  // NO_THREAD_SAFETY_ANALYSIS for static function calling into member function with excludes lock.
  static void* RunProfileSaverThread(void* arg)
      REQUIRES(!Locks::profiler_lock_, !wait_lock_)
      NO_THREAD_SAFETY_ANALYSIS;

  // The run loop for the saver.
  void Run() REQUIRES(!Locks::profiler_lock_, !wait_lock_);

  // Processes the existing profiling info from the jit code cache and returns
  // true if it needed to be saved to disk.
  // If number_of_new_methods is not null, after the call it will contain the number of new methods
  // written to disk.
  // If force_save is true, the saver will ignore any constraints which limit IO (e.g. will write
  // the profile to disk even if it's just one new method).
  bool ProcessProfilingInfo(bool force_save, /*out*/uint16_t* number_of_new_methods)
    REQUIRES(!Locks::profiler_lock_)
    REQUIRES(!Locks::mutator_lock_);

  void NotifyJitActivityInternal() REQUIRES(!wait_lock_);
  void WakeUpSaver() REQUIRES(wait_lock_);

  // Returns true if the saver is shutting down (ProfileSaver::Stop() has been called).
  bool ShuttingDown(Thread* self) REQUIRES(!Locks::profiler_lock_);

  void AddTrackedLocations(const std::string& output_filename,
                           const std::vector<std::string>& code_paths)
      REQUIRES(Locks::profiler_lock_);

  // Fetches the current resolved classes and methods from the ClassLinker and stores them in the
  // profile_cache_ for later save.
  void FetchAndCacheResolvedClassesAndMethods(bool startup);

  void DumpInfo(std::ostream& os);

  // Resolve the realpath of the locations stored in tracked_dex_base_locations_to_be_resolved_
  // and put the result in tracked_dex_base_locations_.
  void ResolveTrackedLocations() REQUIRES(!Locks::profiler_lock_);

  // The only instance of the saver.
  static ProfileSaver* instance_ GUARDED_BY(Locks::profiler_lock_);
  // Profile saver thread.
  static pthread_t profiler_pthread_ GUARDED_BY(Locks::profiler_lock_);

  jit::JitCodeCache* jit_code_cache_;

  // Collection of code paths that the profiler tracks.
  // It maps profile locations to code paths (dex base locations).
  SafeMap<std::string, std::set<std::string>> tracked_dex_base_locations_
      GUARDED_BY(Locks::profiler_lock_);

  // Collection of code paths that the profiler tracks but may note have been resolved
  // to their realpath. The resolution is done async to minimize the time it takes for
  // someone to register a path.
  SafeMap<std::string, std::set<std::string>> tracked_dex_base_locations_to_be_resolved_
      GUARDED_BY(Locks::profiler_lock_);

  bool shutting_down_ GUARDED_BY(Locks::profiler_lock_);
  uint64_t last_time_ns_saver_woke_up_ GUARDED_BY(wait_lock_);
  uint32_t jit_activity_notifications_;

  // A local cache for the profile information. Maps each tracked file to its
  // profile information. This is used to cache the startup classes so that
  // we don't hammer the disk to save them right away.
  // The size of this cache is usually very small and tops
  // to just a few hundreds entries in the ProfileCompilationInfo objects.
  SafeMap<std::string, ProfileCompilationInfo*> profile_cache_;

  // Save period condition support.
  Mutex wait_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable period_condition_ GUARDED_BY(wait_lock_);

  uint64_t total_bytes_written_;
  uint64_t total_number_of_writes_;
  uint64_t total_number_of_code_cache_queries_;
  uint64_t total_number_of_skipped_writes_;
  uint64_t total_number_of_failed_writes_;
  uint64_t total_ms_of_sleep_;
  uint64_t total_ns_of_work_;
  // TODO(calin): replace with an actual size.
  uint64_t max_number_of_profile_entries_cached_;
  uint64_t total_number_of_hot_spikes_;
  uint64_t total_number_of_wake_ups_;

  const ProfileSaverOptions options_;
  DISALLOW_COPY_AND_ASSIGN(ProfileSaver);
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_PROFILE_SAVER_H_
