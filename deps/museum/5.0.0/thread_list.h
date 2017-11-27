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

#ifndef ART_RUNTIME_THREAD_LIST_H_
#define ART_RUNTIME_THREAD_LIST_H_

#include "base/mutex.h"
#include "jni.h"
#include "object_callbacks.h"

#include <bitset>
#include <list>

namespace art {
class Closure;
class Thread;
class TimingLogger;

class ThreadList {
 public:
  static const uint32_t kMaxThreadId = 0xFFFF;
  static const uint32_t kInvalidThreadId = 0;
  static const uint32_t kMainThreadId = 1;

  explicit ThreadList();
  ~ThreadList();

  void DumpForSigQuit(std::ostream& os)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void DumpLocked(std::ostream& os)  // For thread suspend timeout dumps.
      EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  pid_t GetLockOwner();  // For SignalCatcher.

  // Thread suspension support.
  void ResumeAll()
      UNLOCK_FUNCTION(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);
  void Resume(Thread* thread, bool for_debugger = false)
      LOCKS_EXCLUDED(Locks::thread_suspend_count_lock_);

  // Suspends all threads and gets exclusive access to the mutator_lock_.
  void SuspendAll()
      EXCLUSIVE_LOCK_FUNCTION(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);


  // Suspend a thread using a peer, typically used by the debugger. Returns the thread on success,
  // else NULL. The peer is used to identify the thread to avoid races with the thread terminating.
  // If the thread should be suspended then value of request_suspension should be true otherwise
  // the routine will wait for a previous suspend request. If the suspension times out then *timeout
  // is set to true.
  Thread* SuspendThreadByPeer(jobject peer, bool request_suspension, bool debug_suspension,
                              bool* timed_out)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_suspend_thread_lock_)
      LOCKS_EXCLUDED(Locks::mutator_lock_,
                     Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  // Suspend a thread using its thread id, typically used by lock/monitor inflation. Returns the
  // thread on success else NULL. The thread id is used to identify the thread to avoid races with
  // the thread terminating. Note that as thread ids are recycled this may not suspend the expected
  // thread, that may be terminating. If the suspension times out then *timeout is set to true.
  Thread* SuspendThreadByThreadId(uint32_t thread_id, bool debug_suspension, bool* timed_out)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_suspend_thread_lock_)
      LOCKS_EXCLUDED(Locks::mutator_lock_,
                     Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  // Find an already suspended thread (or self) by its id.
  Thread* FindThreadByThreadId(uint32_t thin_lock_id);

  // Run a checkpoint on threads, running threads are not suspended but run the checkpoint inside
  // of the suspend check. Returns how many checkpoints we should expect to run.
  size_t RunCheckpoint(Closure* checkpoint_function)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  size_t RunCheckpointOnRunnableThreads(Closure* checkpoint_function)
      LOCKS_EXCLUDED(Locks::thread_list_lock_, Locks::thread_suspend_count_lock_);

  // Suspends all threads
  void SuspendAllForDebugger()
      LOCKS_EXCLUDED(Locks::mutator_lock_,
                     Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  void SuspendSelfForDebugger()
      LOCKS_EXCLUDED(Locks::thread_suspend_count_lock_);

  void UndoDebuggerSuspensions()
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  // Iterates over all the threads.
  void ForEach(void (*callback)(Thread*, void*), void* context)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_);

  // Add/remove current thread from list.
  void Register(Thread* self)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::runtime_shutdown_lock_)
      LOCKS_EXCLUDED(Locks::mutator_lock_, Locks::thread_list_lock_);
  void Unregister(Thread* self) LOCKS_EXCLUDED(Locks::mutator_lock_, Locks::thread_list_lock_);

  void VisitRoots(RootCallback* callback, void* arg) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void VerifyRoots(VerifyRootCallback* callback, void* arg) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Return a copy of the thread list.
  std::list<Thread*> GetList() EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_) {
    return list_;
  }

  void DumpNativeStacks(std::ostream& os)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);

 private:
  uint32_t AllocThreadId(Thread* self);
  void ReleaseThreadId(Thread* self, uint32_t id) LOCKS_EXCLUDED(Locks::allocated_thread_ids_lock_);

  bool Contains(Thread* thread) EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_);
  bool Contains(pid_t tid) EXCLUSIVE_LOCKS_REQUIRED(Locks::thread_list_lock_);

  void DumpUnattachedThreads(std::ostream& os)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);

  void SuspendAllDaemonThreads()
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);
  void WaitForOtherNonDaemonThreadsToExit()
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  void AssertThreadsAreSuspended(Thread* self, Thread* ignore1, Thread* ignore2 = NULL)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  std::bitset<kMaxThreadId> allocated_ids_ GUARDED_BY(Locks::allocated_thread_ids_lock_);

  // The actual list of all threads.
  std::list<Thread*> list_ GUARDED_BY(Locks::thread_list_lock_);

  // Ongoing suspend all requests, used to ensure threads added to list_ respect SuspendAll.
  int suspend_all_count_ GUARDED_BY(Locks::thread_suspend_count_lock_);
  int debug_suspend_all_count_ GUARDED_BY(Locks::thread_suspend_count_lock_);

  // Signaled when threads terminate. Used to determine when all non-daemons have terminated.
  ConditionVariable thread_exit_cond_ GUARDED_BY(Locks::thread_list_lock_);

  friend class Thread;

  DISALLOW_COPY_AND_ASSIGN(ThreadList);
};

}  // namespace art

#endif  // ART_RUNTIME_THREAD_LIST_H_
