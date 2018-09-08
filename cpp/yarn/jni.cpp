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
#include <vector>

#include <fb/log.h>
#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <jni.h>
#include <profilo/LogEntry.h>
#include <profilo/Logger.h>
#include <yarn/Session.h>

namespace fbjni = facebook::jni;

const char* kPerfSessionType =
    "com/facebook/profilo/provider/yarn/PerfEventsSession";

namespace facebook {
namespace yarn {

static std::vector<EventSpec> providersToSpecs(jboolean majorFaults) {
  auto specs = std::vector<EventSpec>{};
  if (majorFaults) {
    EventSpec spec = {.type = EVENT_TYPE_MAJOR_FAULTS,
                      .tid = EventSpec::kAllThreads};
    specs.push_back(spec);
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

class ProfiloWriterListener : public RecordListener {
 public:
  virtual void onMmap(const RecordMmap& record) {}

  virtual void onSample(const EventType type, const RecordSample& record) {
    if (type == EVENT_TYPE_MAJOR_FAULTS) {
      profilo::Logger::get().write(profilo::entries::StandardEntry{
          .id = 0,
          .type = profilo::entries::MAJOR_FAULT,
          .timestamp = (int64_t)record.time(),
          .tid = (int32_t)record.tid(),
          .callid = 0,
          .matchid = 0,
          .extra = (int64_t)record.addr(),
      });

      FBLOGV("Major Fault: %i %llu", record.tid(), record.addr());
    } else {
      FBLOGV(
          "Unhandled event type: %i (did you configure something that's not implemented yet?)",
          type);
    }
  }

  virtual void onForkEnter(const RecordForkExit& record) {}

  virtual void onForkExit(const RecordForkExit& record) {}

  virtual void onLost(const RecordLost& record) {
    profilo::Logger::get().write(profilo::entries::StandardEntry{
        .id = 0,
        .type = profilo::entries::YARN_LOST_RECORDS,
        .timestamp = profilo::monotonicTime(),
        .tid = profilo::threadID(),
        .callid = 0,
        .matchid = 0,
        .extra = (int64_t)record.lost,
    });
    FBLOGV("Lost records: %u", record.lost);
  }

  virtual void onReaderStop() {}
};
} // namespace

static jlong nativeAttach(
    JNIEnv*,
    jobject cls,
    jboolean majorFaults,
    jint fallbacks,
    jint maxIterations,
    jfloat maxAttachedFdsRatio) {
  auto specs = providersToSpecs(majorFaults);
  if (specs.empty()) {
    throw std::invalid_argument("Could not convert providers");
  }
  if (maxIterations > std::numeric_limits<uint16_t>::max()) {
    throw std::invalid_argument("Max iterations must fit in uint16_t");
  }

  auto session = new Session(
      specs,
      {
          .fallbacks = FALLBACK_RAISE_RLIMIT,
          .maxAttachIterations = static_cast<uint16_t>(maxIterations),
          .maxAttachedFdsRatio = maxAttachedFdsRatio,
      },
      std::unique_ptr<RecordListener>(new ProfiloWriterListener()));
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

static void nativeStart(JNIEnv*, jobject cls, jlong handle) {
  FBLOGV("Session about to run");
  handleToSession(handle)->read();
}

static void nativeStop(JNIEnv*, jobject cls, jlong handle) {
  FBLOGV("Session about to stop");
  handleToSession(handle)->stopRead();
}
} // namespace yarn
} // namespace facebook

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::xplat::initialize(vm, [] {
    fbjni::registerNatives(
        kPerfSessionType,
        {
            makeNativeMethod("nativeAttach", facebook::yarn::nativeAttach),
            makeNativeMethod("nativeDetach", facebook::yarn::nativeDetach),
            makeNativeMethod("nativeStart", facebook::yarn::nativeStart),
            makeNativeMethod("nativeStop", facebook::yarn::nativeStop),
        });
  });
}
