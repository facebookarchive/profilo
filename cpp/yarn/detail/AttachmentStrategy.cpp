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

#include <yarn/detail/AttachmentStrategy.h>

namespace facebook {
namespace yarn {
namespace detail {

static bool tryRaiseFdLimit();
static int getCoreCount();

static int getCoreCount() {
  static const int kNumCores = sysconf(_SC_NPROCESSORS_CONF);
  return kNumCores;
}

PerCoreAttachmentStrategy::PerCoreAttachmentStrategy(
    const EventSpecList& specs,
    uint32_t fallbacks,
    uint16_t max_iterations,
    float open_fds_limit_ratio)
    : specs_(specs), // copy
      global_specs_(0),
      fallbacks_(fallbacks),
      used_fallbacks_(0),
      max_iterations_(max_iterations),
      open_fds_limit_ratio_(open_fds_limit_ratio) {
  size_t global_events = 0; // process-wide events
  for (auto& spec : specs) {
    if (spec.isProcessWide()) {
      ++global_events;
    }
  }
  global_specs_ = global_events;
}

EventList PerCoreAttachmentStrategy::attach() {
  // The list from the previous iteration of the attachment loop,
  // used to calculate the delta from attempt to attempt.
  auto prev_tids = ThreadList();

  // The final list of event objects.
  auto perf_events = EventList();
  bool success = false;

  // The first event on every core becomes the output for all
  // other events on this core. We store their indices into perf_events here.
  // (It's kinda silly but it saves us from using shared_ptr everywhere)
  auto cpu_output_idxs = std::vector<size_t>(getCoreCount());
  auto has_cpu_output = std::vector<bool>(getCoreCount());

  for (int32_t iter = 0; iter < max_iterations_; iter++) {
    auto tids = threadListFromProcFs();
    if (!isWithinLimits(tids.size())) {
      if (tryFallbacks()) {
        iter--; // don't count fallbacks as an attachment iteration
      }
      continue; // try again
    }

    auto events = eventsForDelta(prev_tids, tids);
    for (auto& evt : events) {
      try {
        evt.open();
      } catch (std::system_error& ex) {
        // check for missing thread
        auto current_tids = threadListFromProcFs();
        auto no_result = current_tids.end();
        if (current_tids.find(evt.tid()) == no_result) {
          // Thread is no longer alive, allow this failure. The dead thread
          // remains in `tids`, see comment at the end of the loop.
          continue;
        } else {
          // We don't know what's wrong, rethrow.
          throw;
        }
      }

      perf_events.push_back(std::move(evt));
      size_t last_idx = perf_events.size() - 1;

      // evt is gone now, get a reference to the Event in the list
      auto& list_evt = perf_events.at(last_idx);
      int32_t cpu = list_evt.cpu();
      if (!has_cpu_output[cpu]) {
        // First event on each cpu becomes the "cpu output" - all subsequent
        // events on this core will be redirected to it.
        cpu_output_idxs[cpu] = last_idx;
        has_cpu_output[cpu] = true;
      }
    }

    // If we have at least one process-wide event, we care about attaching to
    // all currently running threads.
    if (global_specs_ > 0) {
      // Get the thread list again and confirm it hasn't changed.
      auto end_tids = threadListFromProcFs();
      if (tids == end_tids) {
        // Same list, reached a fixed point, we're done here.
        success = true;
        break;
      } else {
        // Things changed, record the last list we worked with and try again.
        //
        // It doesn't matter that prev_tids potentially contains threads which
        // are no longer alive (see try-catch above) - that's only a problem
        // if the dead thread's tid is reused and we get a false positive.
        // The chances of tid reusal within two iterations of this loop
        // are infinitesimal.
        prev_tids = std::move(tids);
        continue;
      }
    } else {
      // We are attaching to specific threads and that's all best effort.
      // We don't care if any of the threads suddenly disappeared.
      success = true;
      break;
    }
  }

  if (success) {
    // mmap the cpu leaders and redirect all other events to them.
    for (int cpu = 0; cpu < getCoreCount(); ++cpu) {
      if (!perf_events.empty() && !has_cpu_output[cpu]) {
        throw std::logic_error(
            "Succeeded but did not assign a CPU output event for all cores");
      }

      // TODO: figure out buffer size
      perf_events.at(cpu_output_idxs[cpu]).mmap(4096 * 5);
    }
    for (auto& evt : perf_events) {
      // skip the cpu leaders
      if (evt.buffer() != nullptr) {
        continue;
      }
      auto& cpu_evt = perf_events.at(cpu_output_idxs[evt.cpu()]);
      evt.setOutput(cpu_evt);
    }

    return perf_events;
  } else {
    return EventList();
  }
}

static ThreadList computeDelta(
    const ThreadList& prev_tids,
    const ThreadList& tids) {
  if (prev_tids.empty()) {
    return tids;
  } else {
    auto delta = ThreadList();
    auto no_result = prev_tids.end();
    for (auto& tid : tids) {
      if (prev_tids.find(tid) == no_result) {
        delta.emplace(tid);
      }
    }
    return delta;
  }
}

//
// Build a list of Event objects for all threads in `tids` but not in
// `prev_tids`. If `prev_tids` is nullptr or empty, this will build a list of
// events for every thread in `tids`.
//
EventList PerCoreAttachmentStrategy::eventsForDelta(
    const ThreadList& prev_tids,
    const ThreadList& tids) const {
  auto delta = computeDelta(prev_tids, tids);
  auto events = EventList();
  for (auto& spec : specs_) {
    for (int32_t cpu = 0; cpu < getCoreCount(); cpu++) {
      // one event per core
      if (spec.isProcessWide()) {
        for (auto& tid : delta) {
          // per thread we know about too
          events.emplace_back(spec.type, tid, cpu, true /*inherit*/);
        }
      } else {
        // We're targeting a specific thread but we still
        // need one event per core.
        events.emplace_back(spec.type, spec.tid, cpu, false /*inherit*/);
      }
    }
  }
  return events;
}

bool PerCoreAttachmentStrategy::isWithinLimits(size_t tids_count) {
  auto specific_specs = specs_.size() - global_specs_;

  auto fds = fdListFromProcFs();
  auto fds_count = fds.size();

  auto coreCount = getCoreCount();

  // number of fds we'll add
  auto estimate_new_fds =
      tids_count * coreCount * global_specs_ + // process-global
      coreCount * specific_specs; // specific threads

  // estimated final count
  auto estimate_fds_count = fds_count + estimate_new_fds;

  auto max_fds = getrlimit(RLIMIT_NOFILE);
  auto internal_limit = open_fds_limit_ratio_ * max_fds.rlim_cur;

  return estimate_fds_count <= internal_limit;
}

bool PerCoreAttachmentStrategy::tryFallbacks() {
  if ((fallbacks_ & FALLBACK_RAISE_RLIMIT) &&
      ((used_fallbacks_ & FALLBACK_RAISE_RLIMIT) == 0) && tryRaiseFdLimit()) {
    used_fallbacks_ |= FALLBACK_RAISE_RLIMIT;
    return true;
  }
  return false;
}

// Update the soft file descriptor limit up to the hard limit.
// Returns true if the request succeeded,
// false if they were already the same.
static bool tryRaiseFdLimit() {
  try {
    auto limits = getrlimit(RLIMIT_NOFILE);
    if (limits.rlim_cur == limits.rlim_max) {
      // Soft limit is already up to the hard limit
      return false;
    }

    // Raise the soft limit up to the hard limit
    auto new_limits = rlimit{limits.rlim_max, limits.rlim_max};
    setrlimit(RLIMIT_NOFILE, new_limits);

    // Check if we actually succeeded. If we didn't, we'd keep trying on every
    // attachment iteration but that's okay.
    new_limits = getrlimit(RLIMIT_NOFILE);
    return new_limits.rlim_cur == limits.rlim_max;
  } catch (std::system_error& ex) {
    return false;
  }
}

} // namespace detail
} // namespace yarn
} // namespace facebook
