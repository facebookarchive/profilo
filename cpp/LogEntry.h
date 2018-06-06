/**
 * Copyright 2004-present, Facebook, Inc.
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

#include <cstring>
#include <atomic>
#include <type_traits>
#include <unistd.h>

#include <util/common.h>
#include <profilo/entries/Entry.h>

namespace facebook {
namespace profilo {

enum QuickLogConstants {
  PROF_ERR_SIG_CRASHES = 8126464 | 27,  // = 8126491
  PROF_ERR_SLOT_MISSES = 8126464 | 28,  // = 8126492
  PROF_ERR_STACK_OVERFLOWS = 8126464 | 29,  // = 8126493
  THREAD_CPU_TIME = 9240576 | 5,  // = 9240581
  LOADAVG_1M = 9240576 | 36,  // = 9240612
  LOADAVG_5M = 9240576 | 37,  // = 9240613
  LOADAVG_15M = 9240576 | 38,  // = 9240614
  TOTAL_MEM = 9240576 | 39,  // = 9240615
  FREE_MEM = 9240576 | 40,  // = 9240616
  SHARED_MEM = 9240576 | 41,  // = 9240617
  BUFFER_MEM = 9240576 | 42,  // = 9240618
  NUM_PROCS = 9240576 | 43,  // = 9240619
  QL_THREAD_FAULTS_MAJOR = 9240576 | 6,  // = 9240582
  ALLOC_MMAP_BYTES = 9240576 | 45,  // = 9240621
  ALLOC_MAX_BYTES = 9240576 | 46,  // = 9240622
  ALLOC_TOTAL_BYTES = 9240576 | 47,  // = 9240623
  ALLOC_FREE_BYTES = 9240576 | 48,  // = 9240624
  PROC_CPU_TIME = 9240576 | 3,  // = 9240579
  PROC_SW_FAULTS_MAJOR = 9240576 | 4,  // = 9240580
  THREAD_WAIT_IN_RUNQUEUE_TIME = 9240576 | 50,  // = 9240626
  CONTEXT_SWITCHES_VOLUNTARY = 9240576 | 52,  // = 9240628
  CONTEXT_SWITCHES_INVOLUNTARY = 9240576 | 53,  // = 9240629
  IOWAIT_COUNT = 9240576 | 54,  // = 9240630
  IOWAIT_TIME = 9240576 | 55, // = 9240631
  AVAILABLE_COUNTERS = 8126464 | 37,  // = 8126501
  THREAD_CPU_NUM = 9240576 | 64,  // = 9240640
  CPU_CORE_FREQUENCY = 9240576 | 66,  // = 9240642
  MAX_CPU_CORE_FREQUENCY = 9240576 | 67,  // = 9240643
  PROC_SW_FAULTS_MINOR = 9240576 | 28,  // = 9240604
  THREAD_SW_FAULTS_MINOR = 9240576 | 70,  // = 9240646
  PROC_KERNEL_CPU_TIME = 9240576 | 69,  // = 9240645
  THREAD_KERNEL_CPU_TIME = 9240576 | 68,  // = 9240644
  VMSTAT_NR_DIRTY = 9240576 | 71,  // = 9240647
  VMSTAT_NR_WRITEBACK = 9240576 | 72,  // = 9240648
  VMSTAT_PGPGIN = 9240576 | 73,  // = 9240649
  VMSTAT_PGPGOUT = 9240576 | 74,  // = 9240650
  VMSTAT_ALLOCSTALL = 9240576 | 75,  // = 9240651
  VMSTAT_PAGEOUTRUN = 9240576 | 76,  // = 9240652
  VMSTAT_KSWAPD_STEAL = 9240576 | 77,  // = 9240653
  VMSTAT_PGMAJFAULT = 9240576 | 78,  // = 9240654
  VMSTAT_NR_FREE_PAGES = 9240576 | 79,  // = 9240655
};

} // namespace profilo
} // namespace facebook
