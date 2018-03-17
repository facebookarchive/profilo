/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_THREAD_POOL_H_
#define ART_RUNTIME_THREAD_POOL_H_

#include <deque>
#include <vector>

#include "barrier.h"
#include "base/mutex.h"
#include "mem_map.h"

namespace art {

class ThreadPool;

class Closure {
 public:
  virtual ~Closure() { }
  virtual void Run(Thread* self) = 0;
};

class Task : public Closure {
 public:
  // Called after Closure::Run has been called.
  virtual void Finalize() { }
};

class SelfDeletingTask : public Task {
 public:
  virtual ~SelfDeletingTask() { }
  virtual void Finalize() {
    delete this;
  }
};

class ThreadPoolWorker {
 public:
  static const size_t kDefaultStackSize = 1 * MB;

  size_t GetStackSize() const {
    DCHECK(stack_.get() != nullptr);
    return stack_->Size();
  }

  virtual ~ThreadPoolWorker();

  // Set the "nice" priorty for this worker.
  void SetPthreadPriority(int priority);

 protected:
  ThreadPoolWorker(ThreadPool* thread_pool, const std::string& name, size_t stack_size);
  static void* Callback(void* arg) REQUIRES(!Locks::mutator_lock_);
  virtual void Run();

  ThreadPool* const thread_pool_;
  const std::string name_;
  std::unique_ptr<MemMap> stack_;
  pthread_t pthread_;

 private:
  friend class ThreadPool;
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolWorker);
};

// Note that thread pool workers will set Thread#setCanCallIntoJava to false.
class ThreadPool {
 public:
  // Returns the number of threads in the thread pool.
  size_t GetThreadCount() const {
    return threads_.size();
  }

  // Broadcast to the workers and tell them to empty out the work queue.
  void StartWorkers(Thread* self) REQUIRES(!task_queue_lock_);

  // Do not allow workers to grab any new tasks.
  void StopWorkers(Thread* self) REQUIRES(!task_queue_lock_);

  // Add a new task, the first available started worker will process it. Does not delete the task
  // after running it, it is the caller's responsibility.
  void AddTask(Thread* self, Task* task) REQUIRES(!task_queue_lock_);

  // Remove all tasks in the queue.
  void RemoveAllTasks(Thread* self) REQUIRES(!task_queue_lock_);

  // Create a named thread pool with the given number of threads.
  //
  // If create_peers is true, all worker threads will have a Java peer object. Note that if the
  // pool is asked to do work on the current thread (see Wait), a peer may not be available. Wait
  // will conservatively abort if create_peers and do_work are true.
  ThreadPool(const char* name, size_t num_threads, bool create_peers = false);
  virtual ~ThreadPool();

  // Wait for all tasks currently on queue to get completed.
  // When the pool was created with peers for workers, do_work must not be true (see ThreadPool()).
  void Wait(Thread* self, bool do_work, bool may_hold_locks) REQUIRES(!task_queue_lock_);

  size_t GetTaskCount(Thread* self) REQUIRES(!task_queue_lock_);

  // Returns the total amount of workers waited for tasks.
  uint64_t GetWaitTime() const {
    return total_wait_time_;
  }

  // Provides a way to bound the maximum number of worker threads, threads must be less the the
  // thread count of the thread pool.
  void SetMaxActiveWorkers(size_t threads) REQUIRES(!task_queue_lock_);

  // Set the "nice" priorty for threads in the pool.
  void SetPthreadPriority(int priority);

 protected:
  // get a task to run, blocks if there are no tasks left
  virtual Task* GetTask(Thread* self) REQUIRES(!task_queue_lock_);

  // Try to get a task, returning null if there is none available.
  Task* TryGetTask(Thread* self) REQUIRES(!task_queue_lock_);
  Task* TryGetTaskLocked() REQUIRES(task_queue_lock_);

  // Are we shutting down?
  bool IsShuttingDown() const REQUIRES(task_queue_lock_) {
    return shutting_down_;
  }

  const std::string name_;
  Mutex task_queue_lock_;
  ConditionVariable task_queue_condition_ GUARDED_BY(task_queue_lock_);
  ConditionVariable completion_condition_ GUARDED_BY(task_queue_lock_);
  volatile bool started_ GUARDED_BY(task_queue_lock_);
  volatile bool shutting_down_ GUARDED_BY(task_queue_lock_);
  // How many worker threads are waiting on the condition.
  volatile size_t waiting_count_ GUARDED_BY(task_queue_lock_);
  std::deque<Task*> tasks_ GUARDED_BY(task_queue_lock_);
  // TODO: make this immutable/const?
  std::vector<ThreadPoolWorker*> threads_;
  // Work balance detection.
  uint64_t start_time_ GUARDED_BY(task_queue_lock_);
  uint64_t total_wait_time_;
  Barrier creation_barier_;
  size_t max_active_workers_ GUARDED_BY(task_queue_lock_);
  const bool create_peers_;

 private:
  friend class ThreadPoolWorker;
  friend class WorkStealingWorker;
  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}  // namespace art

#endif  // ART_RUNTIME_THREAD_POOL_H_
