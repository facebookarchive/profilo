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

#ifndef ART_RUNTIME_TRACE_H_
#define ART_RUNTIME_TRACE_H_

#include <bitset>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "atomic.h"
#include "base/macros.h"
#include "globals.h"
#include "instrumentation.h"
#include "os.h"
#include "safe_map.h"

namespace art {

class ArtField;
class ArtMethod;
class DexFile;
class Thread;

using DexIndexBitSet = std::bitset<65536>;
using ThreadIDBitSet = std::bitset<65536>;

enum TracingMode {
  kTracingInactive,
  kMethodTracingActive,
  kSampleProfilingActive,
};

// File format:
//     header
//     record 0
//     record 1
//     ...
//
// Header format:
//     u4  magic ('SLOW')
//     u2  version
//     u2  offset to data
//     u8  start date/time in usec
//     u2  record size in bytes (version >= 2 only)
//     ... padding to 32 bytes
//
// Record format v1:
//     u1  thread ID
//     u4  method ID | method action
//     u4  time delta since start, in usec
//
// Record format v2:
//     u2  thread ID
//     u4  method ID | method action
//     u4  time delta since start, in usec
//
// Record format v3:
//     u2  thread ID
//     u4  method ID | method action
//     u4  time delta since start, in usec
//     u4  wall time since start, in usec (when clock == "dual" only)
//
// 32 bits of microseconds is 70 minutes.
//
// All values are stored in little-endian order.

enum TraceAction {
    kTraceMethodEnter = 0x00,       // method entry
    kTraceMethodExit = 0x01,        // method exit
    kTraceUnroll = 0x02,            // method exited by exception unrolling
    // 0x03 currently unused
    kTraceMethodActionMask = 0x03,  // two bits
};

class Trace FINAL : public instrumentation::InstrumentationListener {
 public:
  enum TraceFlag {
    kTraceCountAllocs = 1,
  };

  enum class TraceOutputMode {
    kFile,
    kDDMS,
    kStreaming
  };

  enum class TraceMode {
    kMethodTracing,
    kSampling
  };

  ~Trace();

  static void SetDefaultClockSource(TraceClockSource clock_source);

  static void Start(const char* trace_filename, int trace_fd, size_t buffer_size, int flags,
                    TraceOutputMode output_mode, TraceMode trace_mode, int interval_us)
      REQUIRES(!Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::thread_suspend_count_lock_,
               !Locks::trace_lock_);
  static void Pause() REQUIRES(!Locks::trace_lock_, !Locks::thread_list_lock_);
  static void Resume() REQUIRES(!Locks::trace_lock_);

  // Stop tracing. This will finish the trace and write it to file/send it via DDMS.
  static void Stop()
      REQUIRES(!Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::trace_lock_);
  // Abort tracing. This will just stop tracing and *not* write/send the collected data.
  static void Abort()
      REQUIRES(!Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::trace_lock_);
  static void Shutdown()
      REQUIRES(!Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::trace_lock_);
  static TracingMode GetMethodTracingMode() REQUIRES(!Locks::trace_lock_);

  bool UseWallClock();
  bool UseThreadCpuClock();
  void MeasureClockOverhead();
  uint32_t GetClockOverheadNanoSeconds();

  void CompareAndUpdateStackTrace(Thread* thread, std::vector<ArtMethod*>* stack_trace)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_, !*streaming_lock_);

  // InstrumentationListener implementation.
  void MethodEntered(Thread* thread, mirror::Object* this_object,
                     ArtMethod* method, uint32_t dex_pc)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_, !*streaming_lock_)
      OVERRIDE;
  void MethodExited(Thread* thread, mirror::Object* this_object,
                    ArtMethod* method, uint32_t dex_pc,
                    const JValue& return_value)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_, !*streaming_lock_)
      OVERRIDE;
  void MethodUnwind(Thread* thread, mirror::Object* this_object,
                    ArtMethod* method, uint32_t dex_pc)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_, !*streaming_lock_)
      OVERRIDE;
  void DexPcMoved(Thread* thread, mirror::Object* this_object,
                  ArtMethod* method, uint32_t new_dex_pc)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_, !*streaming_lock_)
      OVERRIDE;
  void FieldRead(Thread* thread, mirror::Object* this_object,
                 ArtMethod* method, uint32_t dex_pc, ArtField* field)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_) OVERRIDE;
  void FieldWritten(Thread* thread, mirror::Object* this_object,
                    ArtMethod* method, uint32_t dex_pc, ArtField* field,
                    const JValue& field_value)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_) OVERRIDE;
  void ExceptionCaught(Thread* thread, mirror::Throwable* exception_object)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_) OVERRIDE;
  void Branch(Thread* thread, ArtMethod* method, uint32_t dex_pc, int32_t dex_pc_offset)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_) OVERRIDE;
  void InvokeVirtualOrInterface(Thread* thread,
                                mirror::Object* this_object,
                                ArtMethod* caller,
                                uint32_t dex_pc,
                                ArtMethod* callee)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_) OVERRIDE;
  // Reuse an old stack trace if it exists, otherwise allocate a new one.
  static std::vector<ArtMethod*>* AllocStackTrace();
  // Clear and store an old stack trace for later use.
  static void FreeStackTrace(std::vector<ArtMethod*>* stack_trace);
  // Save id and name of a thread before it exits.
  static void StoreExitingThreadInfo(Thread* thread);

  static TraceOutputMode GetOutputMode() REQUIRES(!Locks::trace_lock_);
  static TraceMode GetMode() REQUIRES(!Locks::trace_lock_);
  static size_t GetBufferSize() REQUIRES(!Locks::trace_lock_);

  // Used by class linker to prevent class unloading.
  static bool IsTracingEnabled() REQUIRES(!Locks::trace_lock_);

 private:
  Trace(File* trace_file, const char* trace_name, size_t buffer_size, int flags,
        TraceOutputMode output_mode, TraceMode trace_mode);

  // The sampling interval in microseconds is passed as an argument.
  static void* RunSamplingThread(void* arg) REQUIRES(!Locks::trace_lock_);

  static void StopTracing(bool finish_tracing, bool flush_file)
      REQUIRES(!Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::trace_lock_)
      // There is an annoying issue with static functions that create a new object and call into
      // that object that causes them to not be able to tell that we don't currently hold the lock.
      // This causes the negative annotations to incorrectly have a false positive. TODO: Figure out
      // how to annotate this.
      NO_THREAD_SAFETY_ANALYSIS;
  void FinishTracing() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_);

  void ReadClocks(Thread* thread, uint32_t* thread_clock_diff, uint32_t* wall_clock_diff);

  void LogMethodTraceEvent(Thread* thread, ArtMethod* method,
                           instrumentation::Instrumentation::InstrumentationEvent event,
                           uint32_t thread_clock_diff, uint32_t wall_clock_diff)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_, !*streaming_lock_);

  // Methods to output traced methods and threads.
  void GetVisitedMethods(size_t end_offset, std::set<ArtMethod*>* visited_methods)
      REQUIRES(!*unique_methods_lock_);
  void DumpMethodList(std::ostream& os, const std::set<ArtMethod*>& visited_methods)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_);
  void DumpThreadList(std::ostream& os) REQUIRES(!Locks::thread_list_lock_);

  // Methods to register seen entitites in streaming mode. The methods return true if the entity
  // is newly discovered.
  bool RegisterMethod(ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(streaming_lock_);
  bool RegisterThread(Thread* thread)
      REQUIRES(streaming_lock_);

  // Copy a temporary buffer to the main buffer. Used for streaming. Exposed here for lock
  // annotation.
  void WriteToBuf(const uint8_t* src, size_t src_size)
      REQUIRES(streaming_lock_);

  uint32_t EncodeTraceMethod(ArtMethod* method) REQUIRES(!*unique_methods_lock_);
  uint32_t EncodeTraceMethodAndAction(ArtMethod* method, TraceAction action)
      REQUIRES(!*unique_methods_lock_);
  ArtMethod* DecodeTraceMethod(uint32_t tmid) REQUIRES(!*unique_methods_lock_);
  std::string GetMethodLine(ArtMethod* method) REQUIRES(!*unique_methods_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  void DumpBuf(uint8_t* buf, size_t buf_size, TraceClockSource clock_source)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!*unique_methods_lock_);

  // Singleton instance of the Trace or null when no method tracing is active.
  static Trace* volatile the_trace_ GUARDED_BY(Locks::trace_lock_);

  // The default profiler clock source.
  static TraceClockSource default_clock_source_;

  // Sampling thread, non-zero when sampling.
  static pthread_t sampling_pthread_;

  // Used to remember an unused stack trace to avoid re-allocation during sampling.
  static std::unique_ptr<std::vector<ArtMethod*>> temp_stack_trace_;

  // File to write trace data out to, null if direct to ddms.
  std::unique_ptr<File> trace_file_;

  // Buffer to store trace data.
  std::unique_ptr<uint8_t[]> buf_;

  // Flags enabling extra tracing of things such as alloc counts.
  const int flags_;

  // The kind of output for this tracing.
  const TraceOutputMode trace_output_mode_;

  // The tracing method.
  const TraceMode trace_mode_;

  const TraceClockSource clock_source_;

  // Size of buf_.
  const size_t buffer_size_;

  // Time trace was created.
  const uint64_t start_time_;

  // Clock overhead.
  const uint32_t clock_overhead_ns_;

  // Offset into buf_.
  AtomicInteger cur_offset_;

  // Did we overflow the buffer recording traces?
  bool overflow_;

  // Map of thread ids and names that have already exited.
  SafeMap<pid_t, std::string> exited_threads_;

  // Sampling profiler sampling interval.
  int interval_us_;

  // Streaming mode data.
  std::string streaming_file_name_;
  Mutex* streaming_lock_;
  std::map<const DexFile*, DexIndexBitSet*> seen_methods_;
  std::unique_ptr<ThreadIDBitSet> seen_threads_;

  // Bijective map from ArtMethod* to index.
  // Map from ArtMethod* to index in unique_methods_;
  Mutex* unique_methods_lock_ ACQUIRED_AFTER(streaming_lock_);
  std::unordered_map<ArtMethod*, uint32_t> art_method_id_map_ GUARDED_BY(unique_methods_lock_);
  std::vector<ArtMethod*> unique_methods_ GUARDED_BY(unique_methods_lock_);

  DISALLOW_COPY_AND_ASSIGN(Trace);
};

}  // namespace art

#endif  // ART_RUNTIME_TRACE_H_
