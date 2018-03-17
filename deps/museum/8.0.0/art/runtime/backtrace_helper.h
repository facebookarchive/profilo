/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_BACKTRACE_HELPER_H_
#define ART_RUNTIME_BACKTRACE_HELPER_H_

#include <unwind.h>

namespace art {

// Based on debug malloc logic from libc/bionic/debug_stacktrace.cpp.
class BacktraceCollector {
 public:
  BacktraceCollector(uintptr_t* out_frames, size_t max_depth, size_t skip_count)
      : out_frames_(out_frames), max_depth_(max_depth), skip_count_(skip_count) {}

  size_t NumFrames() const {
    return num_frames_;
  }

  // Collect the backtrace, do not call more than once.
  void Collect() {
    _Unwind_Backtrace(&Callback, this);
  }

 private:
  static _Unwind_Reason_Code Callback(_Unwind_Context* context, void* arg) {
    auto* const state = reinterpret_cast<BacktraceCollector*>(arg);
    const uintptr_t ip = _Unwind_GetIP(context);
    // The first stack frame is get_backtrace itself. Skip it.
    if (ip != 0 && state->skip_count_ > 0) {
      --state->skip_count_;
      return _URC_NO_REASON;
    }
    // ip may be off for ARM but it shouldn't matter since we only use it for hashing.
    state->out_frames_[state->num_frames_] = ip;
    state->num_frames_++;
    return state->num_frames_ >= state->max_depth_ ? _URC_END_OF_STACK : _URC_NO_REASON;
  }

  uintptr_t* const out_frames_ = nullptr;
  size_t num_frames_ = 0u;
  const size_t max_depth_ = 0u;
  size_t skip_count_ = 0u;
};

// A bounded sized backtrace.
template <size_t kMaxFrames>
class FixedSizeBacktrace {
 public:
  void Collect(size_t skip_count) {
    BacktraceCollector collector(frames_, kMaxFrames, skip_count);
    collector.Collect();
    num_frames_ = collector.NumFrames();
  }

  uint64_t Hash() const {
    uint64_t hash = 9314237;
    for (size_t i = 0; i < num_frames_; ++i) {
      hash = hash * 2654435761 + frames_[i];
      hash += (hash >> 13) ^ (hash << 6);
    }
    return hash;
  }

 private:
  uintptr_t frames_[kMaxFrames];
  size_t num_frames_;
};

}  // namespace art

#endif  // ART_RUNTIME_BACKTRACE_HELPER_H_
