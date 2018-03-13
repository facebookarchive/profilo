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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "phaser.h"

#if PHASER_STRATEGY == STRATEGY_FUTEX
#include <linux/futex.h>
#endif

//
// Fundamentally, Phaser is an RCU facility.  A basic understanding of
// Linux kernel RCU is helpful, but not necessary, for the discussion
// below.  What RCU calls "read-side critical sections", we just call
// "critical sections".
//
// Phaser allows arbitrary threads to enter "critical sections" (to
// borrow RCU terminology) using phaser_enter and exit them with
// phaser_exit.  The purpose of phaser_drain is to wait for the
// termination of all critical sections that were active at the
// instant phaser_drain began executing.
//
// Entry and exit from critical sections needs to be fast,
// non-blocking, and completely reentrant.  Note that we need to be
// able to enter a critical section from *inside* phaser_drain, as
// arbitrary signals can arrive during phaser_drain calls.
//
// We optimize for critical sections, since they're much more common
// than phaser_drain calls.  In the common case, phaser_enter is an
// atomic increment and phaser_exit is an atomic decrement.  It's only
// when phaser_drain is running that we need something more complex.
//
// Normally, phaser_enter just increments one of the counter values,
// but if it finds the counter's high bit (the DRAINING bit) set, we
// try incrementing another counter instead.  phaser_drain guarantees
// that at most one counter has a DRAINING bit set, so phaser_enter
// will always be able to find a counter to increment.
//
// phaser_exit decrements the counter phaser_enter incremented.  If
// the DRAINING bit is set on the counter after increment and the
// counter has reached COUNT_DRAINED (which is (DRAINING | 0)), make a
// FUTEX_WAKE system call so that phaser_drain knows it's safe to
// continue.
//
// phaser_drain itself: at the start, from the perspective of the
// thread running phaser_drain, we don't know anything about the
// values of our counters, except that none of them has the DRAINING
// bit set.  We walk through all our counters and drain them by
// setting the DRAINING bit and FUTEX_WAITing for them to go to
// COUNT_DRAINED.  We make sure to drain only one counter at a time,
// so phaser_enter will always be able to make progress; it just might
// have to settle for its second choice of counter.
//
// Architecture notes: on ARM, we only have single-word CAS available,
// so any solution to the problem that involves CAS on double-word
// values won't work.  We try to distribute cacheline updates across
// different cache lines; doing that improves benchmarks by 25% or so.
//
// An earlier version of this code distributed the counts across cache
// lines and was able to speed up a four-thread benchmark by 50%.  A
// factor of two speedup wasn't worth the significant additional
// complexity.
//
// (If the implementation seems trivial, that's because it is:
// most of the work is proving that the trivial implementation
// is correct.
//

#define ARRAY_SIZE(x) (sizeof ((x)) / sizeof ((x)[0]))

#define SIZE_T_HIGH_BIT (SIZE_MAX - (SIZE_MAX >> 1))
static const size_t DRAINING = SIZE_T_HIGH_BIT;
static const size_t COUNT_DRAINED = SIZE_T_HIGH_BIT | 0;

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#if PHASER_STRATEGY == STRATEGY_FUTEX
static int
phaser_futex(void* uaddr, int op, int val,
             const struct timespec* timeout,
             int* uaddr2,
             int val3)
{
  return syscall(__NR_futex, uaddr, op, val, timeout, uaddr2, val3);
}
#endif

int
phaser_init(phaser_t* ph)
{
  memset(ph->counter, 0, sizeof (ph->counter));
#if PHASER_STRATEGY == STRATEGY_PIPE
  int success = pipe(ph->counter_pipe);
  if (success != 0) {
    return -1;
  }
#endif
  return 0;
}

void
phaser_destroy(phaser_t* ph)
{
#if PHASER_STRATEGY == STRATEGY_PIPE
  close(ph->counter_pipe[0]);
  close(ph->counter_pipe[1]);
#endif
}

static bool
phaser_counter_try_inc(size_t* counter)
{
  // N.B. It's very important to try reading the value and test for
  // the DRAINING bit _before_ trying to increment the value.  If we
  // don't, we live-lock once a sufficient number of threads pound on
  // a DRAINING counter and never let it actually reach COUNT_DRAINED.

  if (unlikely(__atomic_load_n(counter, __ATOMIC_RELAXED) & DRAINING)) {
    return false;
  }

  // Use increment, not CAS: XADD is much faster than CMPXCHG on x86,
  // and on ARM it doesn't make a difference.  We can tolerate the
  // race, as explained below.

  __atomic_add_fetch(counter, 1, __ATOMIC_RELAXED);

  // No need to check incremented value.  Yes, phaser_drain races
  // against phaser_enter's check of the DRAINING bit, but we
  // consciously check DRAINING _before_ atomically incrementing the
  // counter, knowing that phaser_drain might set DRAINING between the
  // time of check and time of increment.  That's okay: we'll go on to
  // decrement the counter, and this race can happen only a small
  // number of times.

  return true;
}

phaser_phase
phaser_enter(phaser_t* ph)
{
  phaser_phase nr_phases = ARRAY_SIZE(ph->counter);
  phaser_phase phase = 0;

  while (!phaser_counter_try_inc(&ph->counter[phase])) {
    phase = (phase + 1) & (nr_phases - 1);
  }

  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  return phase;
}

void
phaser_exit(phaser_t* ph, phaser_phase phase)
{
  size_t* counter;
  size_t value;
  int ret;

  __atomic_thread_fence(__ATOMIC_SEQ_CST);

  counter = &ph->counter[phase];
  value = __atomic_add_fetch(counter, -1, __ATOMIC_RELAXED);
  if (unlikely(value == COUNT_DRAINED)) {
#if PHASER_STRATEGY == STRATEGY_FUTEX
    /* Using INT_MAX here is an abundance of caution. The API contract
       limits us to one waiter. */
    ret = phaser_futex(counter, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
    assert(ret != -1);
    (void) ret;
#elif PHASER_STRATEGY == STRATEGY_PIPE
    /* up our "semaphore" */
    char buf = '\0';
    int old_errno = errno;
    do {
      ret = write(ph->counter_pipe[1], &buf, 1);
      if (ret == -1) {
        assert(errno == EINTR);
      }
    } while (ret == -1);
    errno = old_errno;
#else
#error "Unknown phaser strategy"
#endif
  }
}

static void
phaser_drain_counter(phaser_t* ph, phaser_phase phase)
{
  size_t* counter = &ph->counter[phase];
  size_t value;

  value = __atomic_or_fetch(counter, DRAINING, __ATOMIC_RELEASE);
#if PHASER_STRATEGY == STRATEGY_FUTEX
  while (value != COUNT_DRAINED) {
    if (phaser_futex(counter, FUTEX_WAIT, value, NULL, NULL, 0) == -1) {
      assert(errno == EWOULDBLOCK || errno == EINTR);
    }

    __atomic_load(counter, &value, __ATOMIC_RELAXED);
  }

#elif PHASER_STRATEGY == STRATEGY_PIPE
  if (value != COUNT_DRAINED) {
    /* down our "semaphore" */
    int ret;
    char junk;
    do {
      ret = read(ph->counter_pipe[0], &junk, 1);
    } while (ret < 0 && errno == EINTR);
    assert(ret == 1);
  }
#else
#error "Unknown phaser strategy"
#endif
  __atomic_and_fetch(counter, ~DRAINING, __ATOMIC_RELAXED);
}

void
phaser_drain(phaser_t* ph)
{
  for (phaser_phase i = 0; i < ARRAY_SIZE(ph->counter); ++i) {
    phaser_drain_counter(ph, i);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
  }
}
