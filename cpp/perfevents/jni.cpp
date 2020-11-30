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

#include <limits>
#include <tuple>
#include <vector>

#include <fb/log.h>
#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <jni.h>
#include <perfevents/Session.h>
#include <perfevents/detail/ClockOffsetMeasurement.h>
#include <perfevents/detail/FileBackedMappingsList.h>
#include <profilo/LogEntry.h>
#include <profilo/logger/buffer/RingBuffer.h>
#include <util/common.h>

namespace fbjni = facebook::jni;

const char* kPerfSessionType =
    "com/facebook/profilo/provider/perfevents/PerfEventsSession";

namespace facebook {
namespace perfevents {

static std::vector<EventSpec> providersToSpecs(jboolean faults) {
  auto specs = std::vector<EventSpec>{};
  if (faults) {
    EventSpec major_spec = {.type = EVENT_TYPE_MAJOR_FAULTS,
                            .tid = EventSpec::kAllThreads};
    specs.push_back(major_spec);

    EventSpec minor_spec = {.type = EVENT_TYPE_MINOR_FAULTS,
                            .tid = EventSpec::kAllThreads};
    specs.push_back(minor_spec);
  }
  return specs;
}

static Session* handleToSession(jlong handle) {
  if (handle == 0) {
    throw std::invalid_argument("Empty handle passed");
  }
  return reinterpret_cast<Session*>(handle);
}

namespace {

using namespace profilo;
using namespace profilo::logger;
using namespace profilo::entries;

class ProfiloWriterListener : public RecordListener {
  using FileBackedMappingsList = detail::FileBackedMappingsList;

 public:
  ProfiloWriterListener(
      int64_t clock_offset,
      std::vector<EventSpec> const& specs)
      : offset_(clock_offset),
        file_mappings_(buildMappingsFromSpecs(specs)),
        have_filled_mappings_(false) {}

  virtual void onMmap(const RecordMmap& record) {
    if (record.isAnonymous()) {
      return;
    }
    if (file_mappings_) {
      file_mappings_->add(record.addr, record.addr + record.len);
    }
  }

  virtual void onSample(const EventType type, const RecordSample& record) {
    if (file_mappings_ && !have_filled_mappings_) {
      // We fill on first event instead of on FileMappings (or this Listener)
      // construction because this way we know we're attached and won't miss
      // a mapping.
      file_mappings_->fillFromProcMaps();
      have_filled_mappings_ = true;
    }

    switch (type) {
      case EVENT_TYPE_MAJOR_FAULTS: {
        profilo::RingBuffer::get().logger().write(StandardEntry{
            .id = 0,
            .type = EntryType::MAJOR_FAULT,
            .timestamp = ((int64_t)record.time()) + offset_,
            .tid = (int32_t)record.tid(),
            .callid = 0,
            .matchid = 0,
            .extra = (int64_t)record.addr(),
        });
        return;
      }
      case EVENT_TYPE_MINOR_FAULTS: {
        if (file_mappings_ && !file_mappings_->contains(record.addr())) {
          // Ignore anonymous mappings.
          return;
        }

        RingBuffer::get().logger().write(StandardEntry{
            .id = 0,
            .type = EntryType::MINOR_FAULT,
            .timestamp = ((int64_t)record.time()) + offset_,
            .tid = (int32_t)record.tid(),
            .callid = 0,
            .matchid = 0,
            .extra = (int64_t)record.addr(),
        });
        return;
      }
      default: {
      } // ignore
    }
  }

  virtual void onForkEnter(const RecordForkExit& record) {}

  virtual void onForkExit(const RecordForkExit& record) {}

  virtual void onLost(const RecordLost& record) {
    RingBuffer::get().logger().write(StandardEntry{
        .id = 0,
        .type = EntryType::PERFEVENTS_LOST,
        .timestamp = monotonicTime(),
        .tid = threadID(),
        .callid = 0,
        .matchid = 0,
        .extra = (int64_t)record.lost,
    });
    FBLOGV("Lost records: %u", record.lost);
  }

  virtual void onReaderStop() {}

 private:
  int64_t offset_;

  // Contains file-backed mappings, kept up-to-date by
  // virtue of RecordMmap events.
  //
  // First event fills this from /proc/self/maps, after
  // which we add new RecordMmap ranges ourselves.
  std::unique_ptr<FileBackedMappingsList> file_mappings_;
  bool have_filled_mappings_;

  static std::unique_ptr<FileBackedMappingsList> buildMappingsFromSpecs(
      std::vector<EventSpec> const& specs) {
    bool use_mappings = false;
    for (auto& spec : specs) {
      if (spec.type == EVENT_TYPE_MINOR_FAULTS) {
        // We can't keep up with every single minor fault, we need to use
        // the file mappings to filter out the anonymous memory ranges.
        use_mappings = true;
      }
    }
    return use_mappings ? std::make_unique<FileBackedMappingsList>() : nullptr;
  }
};
} // namespace

static jlong nativeAttach(
    JNIEnv*,
    jobject cls,
    jboolean faults,
    jint fallbacks,
    jint maxIterations,
    jfloat maxAttachedFdsRatio) {
  auto specs = providersToSpecs(faults);
  if (specs.empty()) {
    throw std::invalid_argument("Could not convert providers");
  }
  if (maxIterations > std::numeric_limits<uint16_t>::max()) {
    throw std::invalid_argument("Max iterations must fit in uint16_t");
  }

  auto clockOffset = detail::clock::measureOffsetFromPerfClock(CLOCK_MONOTONIC);
  if (clockOffset == INT64_MIN) {
    return 0;
  }

  auto session = new Session(
      specs,
      {
          .fallbacks = FALLBACK_RAISE_RLIMIT,
          .maxAttachIterations = static_cast<uint16_t>(maxIterations),
          .maxAttachedFdsRatio = maxAttachedFdsRatio,
      },
      std::unique_ptr<RecordListener>(
          new ProfiloWriterListener(clockOffset, specs)));

  if (!session->attach()) {
    delete session;
    FBLOGV("Session failed to attach");
    return 0;
  }
  FBLOGV("Session attached");
  return reinterpret_cast<jlong>((void*)session);
}

static void nativeDetach(JNIEnv*, jobject cls, jlong handle) {
  auto session = handleToSession(handle);
  delete session;
}

static void nativeRun(JNIEnv*, jobject cls, jlong handle) {
  FBLOGV("Session about to run");
  handleToSession(handle)->run();
}

static void nativeStop(JNIEnv*, jobject cls, jlong handle) {
  FBLOGV("Session about to stop");
  handleToSession(handle)->stop();
}
} // namespace perfevents
} // namespace facebook

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    fbjni::registerNatives(
        kPerfSessionType,
        {
            makeNativeMethod(
                "nativeAttach", facebook::perfevents::nativeAttach),
            makeNativeMethod(
                "nativeDetach", facebook::perfevents::nativeDetach),
            makeNativeMethod("nativeRun", facebook::perfevents::nativeRun),
            makeNativeMethod("nativeStop", facebook::perfevents::nativeStop),
        });
  });
}
