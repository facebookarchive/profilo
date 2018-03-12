/**
 * Copyright 2018-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

/**
* DESCRIPTION
* -----------
*
* A "phaser" is a synchronization primitive that allows a thread to
* know that all other threads have passed a certain point in
* execution, but without blocking any of these threads.  It is useful
* in cases where callers want to guarantee that a shared resource is
* no longer visible before deallocating it.
*
* Any number of threads can enter and exit a "critical region"
* guarded by a phaser; they use the routines exit phaser_enter and
* phaser_exit.  These functions are non-waiting, non-blocking, fully
* reentrant, infallible, and async-signal-safe.
*
* Another thread (at most one at a time) can concurrently call
* phaser_drain.  This function blocks until all threads that had
* entered the monitored region before the call to phaser_drain make
* the corresponding call to phaser_exit to exit the region.
*
* phaser_enter and phaser_exit are async-signal-safe and completely
* reentrant, making them ideal for use in signal handlers where other
* synchronization primitives may be cumbersome.
*
* PERFORMANCE
* -----------
*
* Phaser is heavily read-oriented.  phaser_enter and phaser_exit are
* atomic increment and atomic decrement in the common case.  On an
* Intel i7-4600U, a phaser_enter and phaser_exit pair completes in
* approximately 28 cycles.  That cost approximately doubles if a
* phaser_drain is running concurrently.
*
* While phaser by itself performs adequately, it occupies only a
* single cache-line, potentially hurting performance in the
* heavily-contended case.  Callers can improve performance in this
* case by a factor of two by using multiple phasers, each situated on
* different cache line.  Threads entering critical sections can choose
* a phaser based on CPU affinity, and a thread wanting synchronize on
* all readers can call phaser_drain on each cacheline's phased in
* turn.
*
* PORTABILITY
* -----------
*
* Phaser relies on the OS providing some kind of wait-on-address
* functionality.  On Linux, we use futex directly.  On Windows, it
* would be possible to use WaitOnAddress.  On FreeBSD, umtx ought to
* work; on iOS, the kernels' psynch_cvwait psynch_cvsignal should
* suffice.
*
* EXAMPLE
* -------
*
* A contrived example may help clarify the use case.  Note that
* worker_threads() performs no heavyweight synchronization and that
* any number of calls to worker_threads() and
* atomically_increment_array() can run concurrently.
*
* std::vector<int>* g_array;
* pthread_mutex_t array_phaser_lock = PTHREAD_MUTEX_INITIALIZER;
* phaser_t array_phaser;
*
* void init()
* {
*   pthread_mutex_init(&array_phaser_lock);
*   phaser_init(&array_phaser);
* }
*
* void worker_threads()
* {
*   phaser_phase phase;
*   for(;;) {
*     phase = phaser_enter(array_phaser);
*     operate_on_array(array);
*     phaser_exit(array_phaser, phase);
*   }
* }
*
* void atomically_increment_array()
* {
*   std::vector<int>* old_array;
*   std::vector<int>* new_array;
*   bool success;
*
*   do {
*     phaser_enter(array_phaser);
*
*     old_array = __atomic_load_n(&g_array, __ATOMIC_ACQUIRE);
*     // NB: in real code, the std::vector constructor can throw,
*     // and calling code should take care to exit the phaser
*     // critical section if it does.
*     new_array = new std::vector<int>(*old_array);
*     for(auto it = new_array->begin(); it != new_array->end(); ++it) {
*       *it += 1;
*     }
*
*     // Important to use __ATOMIC_RELEASE on the success path:
*     // other threads must see only fully-constructed vector.
*     success =
*       __atomic_compare_exchange_n(
*         &g_array,
*         &old_array,
*         new_array,
*         true // weak: we loop anyway,
*         __ATOMIC_RELEASE,
*         __ATOMIC_RELAXED);
*
*     phaser_exit(array_phaser);
*
*     if(!success) {
*       * Someone else beat us to the increment, so try again.
*       delete new_array;
*     }
*   } while(!success);
*
*   // We exclusively own the old array.  Wait for pending readers
*   // to finish.
*
*   pthread_mutex_lock(&array_phaser_lock);
*   phaser_drain(array_phaser);
*   pthread_mutex_unlock(&array_phaser_lock);
*
*   // Now we know that nobody is using old_array: all
*   // references to array occur inside a phaser critical
*   // section, and the call to phaser_drain ensured that
*   // all critical sections that began while g_array still
*   // pointed at old_array have now terminated.
*
*   delete old_array;
* }
*/

#ifdef __cplusplus
extern "C" {
#endif

#define STRATEGY_FUTEX 1
#define STRATEGY_PIPE 2

#ifndef PHASER_STRATEGY
#ifdef __linux__
#define PHASER_STRATEGY STRATEGY_FUTEX
#else
#define PHASER_STRATEGY STRATEGY_PIPE
#endif
#endif

#if PHASER_STRATEGY == STRATEGY_FUTEX
#define PHASER_UNINITIALIZED { { 0, 0 } }
#elif PHASER_STRATEGY == STRATEGY_PIPE
#define PHASER_UNINITIALIZED { { 0, 0 }, { -1, -1 } }
#else
#error "Unknown phaser strategy"
#endif

#define NUM_PHASES 2 /* must be power of two! */

typedef struct phaser {
  size_t counter[NUM_PHASES];
#if PHASER_STRATEGY == STRATEGY_PIPE
  /* We don't have futex on non-Linux, so we fall back to using a pipe
     as a semaphore: down = read a byte, up = write a byte. We only
     need one pipe because we only drain one counter at a time. */
  int counter_pipe[2];
#endif
} phaser_t;

typedef uint32_t phaser_phase;

/**
 * Initialize a phaser object. This function must be paired with
 * phaser_destroy.
 * Returns 0 on success, nonzero on failure. On failure, errno is set.
 */
int phaser_init(phaser_t* ph);

/**
 * De-initialize a phaser object. ph must previously have been
 * successfully initialized with phaser_init.
 */
void phaser_destroy(phaser_t* ph);

/**
 * Enter a phaser critical region.  This function must be paired with
 * phaser_exit.  Return a token to pass to phaser_exit.  This function
 * is reentrant and async-signal-safe: it may be called even on a
 * thread currently executing phaser_drain (e.g., from a signal
 * handler).  This function cannot fail and does not block or wait.
 *
 * phaser_enter is a full memory barrier.
 */
__attribute__ ((warn_unused_result))
phaser_phase phaser_enter(phaser_t* ph);

/**
 * Exit a phaser critical region.  This function is reentrant and
 * async-signal-safe.  PHASE is the return value of the matching
 * phaser_enter call.
 *
 * phaser_exit is a full memory barrier.
 */
void phaser_exit(phaser_t* ph, phaser_phase phase);

/**
 * Block and wait for all active critical regions on PH to exit.
 * (That is, wait for the phaser_exit calls corresponding to any
 * unpaired phaser_enter calls on PH.)  This routine is *not*
 * async-signal-safe.  Do not call it while the current thread is PH's
 * critical region.  phaser_drain may or may not wait for the end of
 * some critical regions that begin while a phaser_drain call is in
 * active, but phaser_drain is guaranteed to make forward progress and
 * complete in finite time, assuming critical sections do the same.
 *
 * Callers must serialize phaser_drain calls on a given phaser object.
 * phaser_drain is a full memory barrier and a pthread cancellation
 * point.
 */
void phaser_drain(phaser_t* ph);

#ifdef __cplusplus
} // extern "C"
#endif
