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

#ifndef ART_RUNTIME_GC_TASK_PROCESSOR_H_
#define ART_RUNTIME_GC_TASK_PROCESSOR_H_

#include <memory>
#include <set>

#include "base/mutex.h"
#include "globals.h"
#include "thread_pool.h"

namespace art {
namespace gc {

class HeapTask : public SelfDeletingTask {
 public:
  explicit HeapTask(uint64_t target_run_time) : target_run_time_(target_run_time) {
  }
  uint64_t GetTargetRunTime() const {
    return target_run_time_;
  }

 private:
  // Update the updated_target_run_time_, the task processor will re-insert the task when it is
  // popped and update the target_run_time_.
  void SetTargetRunTime(uint64_t new_target_run_time) {
    target_run_time_ = new_target_run_time;
  }

  // Time in ns at which we want the task to run.
  uint64_t target_run_time_;

  friend class TaskProcessor;
  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapTask);
};

// Used to process GC tasks (heap trim, heap transitions, concurrent GC).
class TaskProcessor {
 public:
  TaskProcessor();
  virtual ~TaskProcessor();
  void AddTask(Thread* self, HeapTask* task) REQUIRES(!*lock_);
  HeapTask* GetTask(Thread* self) REQUIRES(!*lock_);
  void Start(Thread* self) REQUIRES(!*lock_);
  // Stop tells the RunAllTasks to finish up the remaining tasks as soon as
  // possible then return.
  void Stop(Thread* self) REQUIRES(!*lock_);
  void RunAllTasks(Thread* self) REQUIRES(!*lock_);
  bool IsRunning() const REQUIRES(!*lock_);
  void UpdateTargetRunTime(Thread* self, HeapTask* target_time, uint64_t new_target_time)
      REQUIRES(!*lock_);
  Thread* GetRunningThread() const REQUIRES(!*lock_);

 private:
  class CompareByTargetRunTime {
   public:
    bool operator()(const HeapTask* a, const HeapTask* b) const {
      return a->GetTargetRunTime() < b->GetTargetRunTime();
    }
  };

  mutable Mutex* lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  bool is_running_ GUARDED_BY(lock_);
  std::unique_ptr<ConditionVariable> cond_ GUARDED_BY(lock_);
  std::multiset<HeapTask*, CompareByTargetRunTime> tasks_ GUARDED_BY(lock_);
  Thread* running_thread_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(TaskProcessor);
};

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_TASK_PROCESSOR_H_
