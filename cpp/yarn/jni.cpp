// Copyright 2004-present Facebook. All Rights Reserved.

#include <limits>
#include <vector>

#include <jni.h>
#include <fbjni/fbjni.h>
#include <fb/log.h>
#include <fb/xplat_init.h>
#include <loom/LogEntry.h>
#include <loom/Logger.h>
#include <yarn/Session.h>

namespace fbjni = facebook::jni;

const char* kPerfSessionType = "com/facebook/loom/provider/yarn/PerfEventsSession";

namespace facebook {
namespace yarn {

static std::vector<EventSpec> providersToSpecs(jint providers) {
  auto specs = std::vector<EventSpec>{};
  if ((providers & loom::PROVIDER_MAJOR_FAULTS) != 0) {
    EventSpec spec = {
      .type = EVENT_TYPE_MAJOR_FAULTS,
      .tid = EventSpec::kAllThreads
    };
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

class LoomWriterListener: public RecordListener {
public:
  virtual void onMmap(const RecordMmap& record) {
  }

  virtual void onSample(const EventType type, const RecordSample& record) {
    if (type == EVENT_TYPE_MAJOR_FAULTS) {
      loom::Logger::get().write(
        loom::entries::StandardEntry {
          .id = 0,
          .type = loom::entries::MAJOR_FAULT,
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

  virtual void onForkEnter(const RecordForkExit& record) {
  }

  virtual void onForkExit(const RecordForkExit& record) {
  }

  virtual void onLost(const RecordLost& record) {
    loom::Logger::get().write(
      loom::entries::StandardEntry {
        .id = 0,
        .type = loom::entries::YARN_LOST_RECORDS,
        .timestamp = loom::monotonicTime(),
        .tid = loom::threadID(),
        .callid = 0,
        .matchid = 0,
        .extra = (int64_t)record.lost,
      });
    FBLOGV("Lost records: %u", record.lost);
  }

  virtual void onReaderStop() {
  }
};
}

static jlong nativeAttach(
  JNIEnv*,
  jobject cls,
  jint providers,
  jint fallbacks,
  jint maxIterations,
  jfloat maxAttachedFdsRatio
){
  auto specs = providersToSpecs(providers);
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
    std::unique_ptr<RecordListener>(new LoomWriterListener())
  );
  if (!session->attach()) {
    delete session;
    FBLOGV("Session failed to attach");
    return 0;
  }
  FBLOGV("Session attached");
  return reinterpret_cast<jlong>((void*) session);
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
} // yarn
} // facebook

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
