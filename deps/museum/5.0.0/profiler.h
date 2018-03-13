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

#ifndef ART_RUNTIME_PROFILER_H_
#define ART_RUNTIME_PROFILER_H_

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "barrier.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "globals.h"
#include "instrumentation.h"
#include "profiler_options.h"
#include "os.h"
#include "safe_map.h"
#include "method_reference.h"

namespace art {

namespace mirror {
  class ArtMethod;
  class Class;
}  // namespace mirror
class Thread;

typedef std::pair<mirror::ArtMethod*, uint32_t> InstructionLocation;

// This class stores the sampled bounded stacks in a trie structure. A path of the trie represents
// a particular context with the method on top of the stack being a leaf or an internal node of the
// trie rather than the root.
class StackTrieNode {
 public:
  StackTrieNode(MethodReference method, uint32_t dex_pc, uint32_t method_size,
      StackTrieNode* parent) :
      parent_(parent), method_(method), dex_pc_(dex_pc),
      count_(0), method_size_(method_size) {
  }
  StackTrieNode() : parent_(nullptr), method_(nullptr, 0),
      dex_pc_(0), count_(0), method_size_(0) {
  }
  StackTrieNode* GetParent() { return parent_; }
  MethodReference GetMethod() { return method_; }
  uint32_t GetCount() { return count_; }
  uint32_t GetDexPC() { return dex_pc_; }
  uint32_t GetMethodSize() { return method_size_; }
  void AppendChild(StackTrieNode* child) { children_.insert(child); }
  StackTrieNode* FindChild(MethodReference method, uint32_t dex_pc);
  void DeleteChildren();
  void IncreaseCount() { ++count_; }

 private:
  // Comparator for stack trie node.
  struct StackTrieNodeComparator {
    bool operator()(StackTrieNode* node1, StackTrieNode* node2) const {
      MethodReference mr1 = node1->GetMethod();
      MethodReference mr2 = node2->GetMethod();
      if (mr1.dex_file == mr2.dex_file) {
        if (mr1.dex_method_index == mr2.dex_method_index) {
          return node1->GetDexPC() < node2->GetDexPC();
        } else {
          return mr1.dex_method_index < mr2.dex_method_index;
        }
      } else {
        return mr1.dex_file < mr2.dex_file;
      }
    }
  };

  std::set<StackTrieNode*, StackTrieNodeComparator> children_;
  StackTrieNode* parent_;
  MethodReference method_;
  uint32_t dex_pc_;
  uint32_t count_;
  uint32_t method_size_;
};

//
// This class holds all the results for all runs of the profiler.  It also
// counts the number of null methods (where we can't determine the method) and
// the number of methods in the boot path (where we have already compiled the method).
//
// This object is an internal profiler object and uses the same locking as the profiler
// itself.
class ProfileSampleResults {
 public:
  explicit ProfileSampleResults(Mutex& lock);
  ~ProfileSampleResults();

  void Put(mirror::ArtMethod* method);
  void PutStack(const std::vector<InstructionLocation>& stack_dump);
  uint32_t Write(std::ostream &os, ProfileDataType type);
  void ReadPrevious(int fd, ProfileDataType type);
  void Clear();
  uint32_t GetNumSamples() { return num_samples_; }
  void NullMethod() { ++num_null_methods_; }
  void BootMethod() { ++num_boot_methods_; }

 private:
  uint32_t Hash(mirror::ArtMethod* method);
  static constexpr int kHashSize = 17;
  Mutex& lock_;                  // Reference to the main profiler lock - we don't need two of them.
  uint32_t num_samples_;         // Total number of samples taken.
  uint32_t num_null_methods_;    // Number of samples where can don't know the method.
  uint32_t num_boot_methods_;    // Number of samples in the boot path.

  typedef std::map<mirror::ArtMethod*, uint32_t> Map;  // Map of method vs its count.
  Map *table[kHashSize];

  typedef std::set<StackTrieNode*> TrieNodeSet;
  // Map of method hit by profiler vs the set of stack trie nodes for this method.
  typedef std::map<MethodReference, TrieNodeSet*, MethodReferenceComparator> MethodContextMap;
  MethodContextMap *method_context_table;
  StackTrieNode* stack_trie_root_;  // Root of the trie that stores sampled stack information.

  // Map from <pc, context> to counts.
  typedef std::map<std::pair<uint32_t, std::string>, uint32_t> PreviousContextMap;
  struct PreviousValue {
    PreviousValue() : count_(0), method_size_(0), context_map_(nullptr) {}
    PreviousValue(uint32_t count, uint32_t method_size, PreviousContextMap* context_map)
      : count_(count), method_size_(method_size), context_map_(context_map) {}
    uint32_t count_;
    uint32_t method_size_;
    PreviousContextMap* context_map_;
  };

  typedef std::map<std::string, PreviousValue> PreviousProfile;
  PreviousProfile previous_;
  uint32_t previous_num_samples_;
  uint32_t previous_num_null_methods_;     // Number of samples where can don't know the method.
  uint32_t previous_num_boot_methods_;     // Number of samples in the boot path.
};

//
// The BackgroundMethodSamplingProfiler runs in a thread.  Most of the time it is sleeping but
// occasionally wakes up and counts the number of times a method is called.  Each time
// it ticks, it looks at the current method and records it in the ProfileSampleResults
// table.
//
// The timing is controlled by a number of variables:
// 1.  Period: the time between sampling runs.
// 2.  Interval: the time between each sample in a run.
// 3.  Duration: the duration of a run.
//
// So the profiler thread is sleeping for the 'period' time.  It wakes up and runs for the
// 'duration'.  The run consists of a series of samples, each of which is 'interval' microseconds
// apart.  At the end of a run, it writes the results table to a file and goes back to sleep.

class BackgroundMethodSamplingProfiler {
 public:
  // Start a profile thread with the user-supplied arguments.
  // Returns true if the profile was started or if it was already running. Returns false otherwise.
  static bool Start(const std::string& output_filename, const ProfilerOptions& options)
  LOCKS_EXCLUDED(Locks::mutator_lock_,
                 Locks::thread_list_lock_,
                 Locks::thread_suspend_count_lock_,
                 Locks::profiler_lock_);

  static void Stop() LOCKS_EXCLUDED(Locks::profiler_lock_, wait_lock_);
  static void Shutdown() LOCKS_EXCLUDED(Locks::profiler_lock_);

  void RecordMethod(mirror::ArtMethod *method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void RecordStack(const std::vector<InstructionLocation>& stack) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool ProcessMethod(mirror::ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  const ProfilerOptions& GetProfilerOptions() const { return options_; }

  Barrier& GetBarrier() {
    return *profiler_barrier_;
  }

 private:
  explicit BackgroundMethodSamplingProfiler(
    const std::string& output_filename, const ProfilerOptions& options);

  // The sampling interval in microseconds is passed as an argument.
  static void* RunProfilerThread(void* arg) LOCKS_EXCLUDED(Locks::profiler_lock_);

  uint32_t WriteProfile() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void CleanProfile();
  uint32_t DumpProfile(std::ostream& os) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static bool ShuttingDown(Thread* self) LOCKS_EXCLUDED(Locks::profiler_lock_);

  static BackgroundMethodSamplingProfiler* profiler_ GUARDED_BY(Locks::profiler_lock_);

  // We need to shut the sample thread down at exit.  Setting this to true will do that.
  static volatile bool shutting_down_ GUARDED_BY(Locks::profiler_lock_);

  // Sampling thread, non-zero when sampling.
  static pthread_t profiler_pthread_;

  // Some measure of the number of samples that are significant.
  static constexpr uint32_t kSignificantSamples = 10;

  // The name of the file where profile data will be written.
  std::string output_filename_;
  // The options used to start the profiler.
  const ProfilerOptions& options_;


  // Profile condition support.
  Mutex wait_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable period_condition_ GUARDED_BY(wait_lock_);

  ProfileSampleResults profile_table_;

  std::unique_ptr<Barrier> profiler_barrier_;

  // Set of methods to be filtered out.  This will probably be rare because
  // most of the methods we want to be filtered reside in the boot path and
  // are automatically filtered.
  typedef std::set<std::string> FilteredMethods;
  FilteredMethods filtered_methods_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundMethodSamplingProfiler);
};

//
// Contains profile data generated from previous runs of the program and stored
// in a file.  It is used to determine whether to compile a particular method or not.
class ProfileFile {
 public:
  class ProfileData {
   public:
    ProfileData() : count_(0), method_size_(0), used_percent_(0) {}
    ProfileData(const std::string& method_name, uint32_t count, uint32_t method_size,
      double used_percent, double top_k_used_percentage) :
      method_name_(method_name), count_(count), method_size_(method_size),
      used_percent_(used_percent), top_k_used_percentage_(top_k_used_percentage) {
      // TODO: currently method_size_ is unused
      UNUSED(method_size_);
    }

    double GetUsedPercent() const { return used_percent_; }
    uint32_t GetCount() const { return count_; }
    double GetTopKUsedPercentage() const { return top_k_used_percentage_; }

   private:
    std::string method_name_;       // Method name.
    uint32_t count_;                // Number of times it has been called.
    uint32_t method_size_;          // Size of the method on dex instructions.
    double used_percent_;           // Percentage of how many times this method was called.
    double top_k_used_percentage_;  // The percentage of the group that comprise K% of the total
                                    // used methods this methods belongs to.
  };

 public:
  // Loads profile data from the given file. The new data are merged with any existing data.
  // Returns true if the file was loaded successfully and false otherwise.
  bool LoadFile(const std::string& filename);

  // Computes the group that comprise top_k_percentage of the total used methods.
  bool GetTopKSamples(std::set<std::string>& top_k_methods, double top_k_percentage);

  // If the given method has an entry in the profile table it updates the data
  // and returns true. Otherwise returns false and leaves the data unchanged.
  bool GetProfileData(ProfileData* data, const std::string& method_name);

 private:
  // Profile data is stored in a map, indexed by the full method name.
  typedef std::map<std::string, ProfileData> ProfileMap;
  ProfileMap profile_map_;
};

}  // namespace art

#endif  // ART_RUNTIME_PROFILER_H_
