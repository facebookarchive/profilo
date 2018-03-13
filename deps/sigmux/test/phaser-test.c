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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include <pthread.h>
#include <sys/mman.h>
#ifdef __MACH__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif
#include "phaser.h"

static pthread_key_t statkey;

struct stats {
  size_t count;
};

phaser_t stat_phaser;
struct stats* stats;

#define NANOS_PER_SEC 1000000000

static struct timespec
now(void)
{
#ifdef __MACH__
  struct timespec n;
  // REVIEW: we could get fancy with pthread_once, but it's a test, so
  // 2 syscalls instead of 1 seems fine.
  struct mach_timebase_info tb_info = {0};
  mach_timebase_info(&tb_info);
  uint64_t nanos = mach_absolute_time() * tb_info.numer / tb_info.denom;
  n.tv_nsec = nanos % NANOS_PER_SEC;
  n.tv_sec = nanos / NANOS_PER_SEC;
  return n;
#else
  struct timespec n;
  clock_gettime(CLOCK_MONOTONIC, &n);
  return n;
#endif
}

static double
ts_to_seconds(struct timespec t)
{
  return (double) t.tv_sec + (double) t.tv_nsec / 1e9;
}

static double
elapsed(struct timespec b, struct timespec a)
{
  return ts_to_seconds(b) - ts_to_seconds(a);
}


static pthread_t
xpthread_create(void* (*thfunc)(void*))
{
  pthread_t child;
  if (pthread_create(&child, NULL, thfunc, NULL) != 0) {
    abort();
  }

  return child;
}

static void*
phaser_thread(void* arg)
{
  struct stats* s;
  unsigned loopcount = 0;
  unsigned threshold = 10000;
  phaser_phase phase;

  for (;;) {
    phase = phaser_enter(&stat_phaser);
    // Only update the count once in a while in order to avoid making
    // the cacheline bounce around so much.  Increases benchmark
    // jitter, but not by enough to matter.
    if (++loopcount == threshold) {
      s = __atomic_load_n(&stats, __ATOMIC_ACQUIRE);
      __atomic_add_fetch(&s->count, loopcount, __ATOMIC_RELEASE);
      loopcount = 0;
    }
    phaser_exit(&stat_phaser, phase);
  }

  return NULL;
}

static struct stats*
stats_alloc()
{
  // Use mmap to maximize probably of catching accesses outside critical region
  // Yes, MAP_ANON is deprecated, but it's a synonym for MAP_ANONYMOUS
  // and only the former is available on iOS/OSX.
  struct stats* s = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
  if (s == NULL) {
    fprintf(stderr, "address space exhausted\n");
    _exit(1);
  }

  return s;
}

void
stats_dealloc(struct stats* s)
{
  // munmap(s, 4096); Keep the address space reserved so that we mmap
  // never gives us the same address space, making sure we catch
  // access outside critical region.
  mprotect(s, 4096, PROT_NONE);
}

static void*
mutator_thread(void* arg)
{
  struct stats* snew;
  struct stats* sold;
  struct timespec start, end;

  for (;;) {
    sleep(5);
    snew = stats_alloc();
    sold = __atomic_exchange_n(&stats, snew, __ATOMIC_RELAXED);
    start = now();
    phaser_drain(&stat_phaser);
    end = now();
    // Now we exclusively own sold
    if (sold->count != 0) {
      printf("sold->count: %e delay: %e sec\n",
             (double) sold->count,
             elapsed(end, start));
      stats_dealloc(sold);
    }
  }

  return NULL;
}

int
main(int argc, char** argv)
{
  pthread_key_create(&statkey, NULL);
  phaser_init(&stat_phaser);

  int nr_threads = 10;

  if (argv[1]) {
    nr_threads = atoi(argv[1]);
  }

  printf("starting benchmark nr_threads=%d\n", nr_threads);

  pthread_t mutator;
  pthread_t children[nr_threads];
  stats = stats_alloc();

  mutator = xpthread_create(mutator_thread);
  (void) mutator;

  for (unsigned i = 0; i < nr_threads; ++i) {
    children[i] = xpthread_create(phaser_thread);
  }

  for (unsigned i = 0; i < nr_threads; ++i) {
    pthread_join(children[i], NULL);
  }

  return 0;
}
