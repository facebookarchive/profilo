"""
Copyright 2018-present, Facebook, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""


COUNTER_NAMES = {
    9240581: "THREAD_CPU_TIME",
    9240612: "LOADAVG_1M",
    9240613: "LOADAVG_5M",
    9240614: "LOADAVG_15M",
    9240615: "TOTAL_MEM",
    9240616: "FREE_MEM",
    9240617: "SHARED_MEM",
    9240618: "BUFFER_MEM",
    9240619: "NUM_PROCS",
    9240582: "QL_THREAD_FAULTS_MAJOR",
    9240621: "ALLOC_MMAP_BYTES",
    9240622: "ALLOC_MAX_BYTES",
    9240623: "ALLOC_TOTAL_BYTES",
    9240624: "ALLOC_FREE_BYTES",
    9240579: "PROC_CPU_TIME",
    9240580: "PROC_SW_FAULTS_MAJOR",
    9240593: "GLOBAL_ALLOC_COUNT",
    9240594: "GLOBAL_ALLOC_SIZE",
    9240595: "GLOBAL_GC_INVOCATION_SIZE",
    9240626: "THREAD_WAIT_IN_RUNQUEUE_TIME",
    9240628: "CONTEXT_SWITCHES_VOLUNTARY",
    9240629: "CONTEXT_SWITCHES_INVOLUNTARY",
    9240630: "IOWAIT_COUNT",
    9240631: "IOWAIT_TIME",
    8126501: "AVAILABLE_COUNTERS",
    9240634: "JAVA_FREE_BYTES",
    9240635: "JAVA_MAX_BYTES",
    9240636: "JAVA_ALLOC_BYTES",
    9240637: "JAVA_TOTAL_BYTES",
}


ANNOTATION_NAMES = {
    8126491: "PROF_ERR_SIG_CRASHES",
    8126492: "PROF_ERR_SLOT_MISSES",
    8126493: "PROF_ERR_STACK_OVERFLOWS",
}
