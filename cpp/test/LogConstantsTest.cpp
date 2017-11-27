// Copyright 2004-present Facebook. All Rights Reserved.

#include <jni.h>
#include <fb/fbjni.h>

#include <loom/LogEntry.h>

static constexpr const char* kTestClassJavaName = "com/facebook/loom/logger/LogConstantsTest";

using namespace facebook::jni;
using namespace facebook::loom;

static void expectTrue(bool val, const std::string& name) {
  if (!val) {
    throw std::runtime_error(name + " doesn't match");
  }
}

static void verifyStaticField(alias_ref<jclass> clazz, const std::string& field_name, int value) {
  auto fld = clazz->getStaticField<jint>(field_name.data());
  int fieldValue = clazz->getStaticFieldValue(fld);
  expectTrue(fieldValue == value, field_name);
}

void verifyLoomConstants(alias_ref<jobject> self, alias_ref<jclass> clazz) {
  verifyStaticField(clazz, "PROVIDER_ASYNC", LogProvider::PROVIDER_ASYNC);
  verifyStaticField(clazz, "PROVIDER_LIFECYCLE", LogProvider::PROVIDER_LIFECYCLE);
  verifyStaticField(clazz, "PROVIDER_QPL", LogProvider::PROVIDER_QPL);
  verifyStaticField(clazz, "PROVIDER_OTHER", LogProvider::PROVIDER_OTHER);
  verifyStaticField(clazz, "PROVIDER_USER_COUNTERS", LogProvider::PROVIDER_USER_COUNTER);
  verifyStaticField(clazz, "PROVIDER_SYSTEM_COUNTERS", LogProvider::PROVIDER_SYSTEM_COUNTERS);
  verifyStaticField(clazz, "PROVIDER_STACK_FRAME", LogProvider::PROVIDER_STACK_FRAME);
  verifyStaticField(clazz, "PROVIDER_LIGER", LogProvider::PROVIDER_LIGER);
  verifyStaticField(clazz, "PROVIDER_MAJOR_FAULTS", LogProvider::PROVIDER_MAJOR_FAULTS);
  verifyStaticField(clazz, "PROVIDER_THREAD_SCHEDULE", LogProvider::PROVIDER_THREAD_SCHEDULE);
  verifyStaticField(clazz, "PROVIDER_CLASS_LOAD", LogProvider::PROVIDER_CLASS_LOAD);
  verifyStaticField(clazz, "PROVIDER_NATIVE_STACK_TRACE", LogProvider::PROVIDER_NATIVE_STACK_TRACE);
  verifyStaticField(clazz, "PROVIDER_MAIN_THREAD_MESSAGES", LogProvider::PROVIDER_MAIN_THREAD_MESSAGES);
  verifyStaticField(clazz, "PROVIDER_LIGER_HTTP2", LogProvider::PROVIDER_LIGER_HTTP2);
  verifyStaticField(clazz, "PROVIDER_FBSYSTRACE", LogProvider::PROVIDER_FBSYSTRACE);
}

void verifyQuickLogLoomConstants(alias_ref<jobject> self, alias_ref<jclass> clazz) {
  verifyStaticField(clazz, "PROF_ERR_SIG_CRASHES", LoomQuickLogConstants::PROF_ERR_SIG_CRASHES);
  verifyStaticField(clazz, "PROF_ERR_SLOT_MISSES", LoomQuickLogConstants::PROF_ERR_SLOT_MISSES);
  verifyStaticField(clazz, "PROF_ERR_STACK_OVERFLOWS", LoomQuickLogConstants::PROF_ERR_STACK_OVERFLOWS);
  verifyStaticField(clazz, "AVAILABLE_COUNTERS", LoomQuickLogConstants::AVAILABLE_COUNTERS);
}

void verifyQuickLogProcAndPerfConstants(alias_ref<jobject> self, alias_ref<jclass> clazz) {
  verifyStaticField(clazz, "THREAD_CPU_TIME", LoomQuickLogConstants::THREAD_CPU_TIME);
  verifyStaticField(clazz, "LOADAVG_1M", LoomQuickLogConstants::LOADAVG_1M);
  verifyStaticField(clazz, "LOADAVG_5M", LoomQuickLogConstants::LOADAVG_5M);
  verifyStaticField(clazz, "LOADAVG_15M", LoomQuickLogConstants::LOADAVG_15M);
  verifyStaticField(clazz, "TOTAL_MEM", LoomQuickLogConstants::TOTAL_MEM);
  verifyStaticField(clazz, "FREE_MEM", LoomQuickLogConstants::FREE_MEM);
  verifyStaticField(clazz, "SHARED_MEM", LoomQuickLogConstants::SHARED_MEM);
  verifyStaticField(clazz, "BUFFER_MEM", LoomQuickLogConstants::BUFFER_MEM);
  verifyStaticField(clazz, "NUM_PROCS", LoomQuickLogConstants::NUM_PROCS);
  verifyStaticField(clazz, "THREAD_SW_FAULTS_MAJOR", LoomQuickLogConstants::QL_THREAD_FAULTS_MAJOR);
  verifyStaticField(clazz, "THREAD_WAIT_IN_RUNQUEUE_TIME", LoomQuickLogConstants::THREAD_WAIT_IN_RUNQUEUE_TIME);
  verifyStaticField(clazz, "CONTEXT_SWITCHES_VOLUNTARY", LoomQuickLogConstants::CONTEXT_SWITCHES_VOLUNTARY);
  verifyStaticField(clazz, "CONTEXT_SWITCHES_INVOLUNTARY", LoomQuickLogConstants::CONTEXT_SWITCHES_INVOLUNTARY);
  verifyStaticField(clazz, "IOWAIT_COUNT", LoomQuickLogConstants::IOWAIT_COUNT);
  verifyStaticField(clazz, "IOWAIT_TIME", LoomQuickLogConstants::IOWAIT_TIME);
}

jint JNI_OnLoad(JavaVM* vm, void*) {
  return initialize(vm, [] {
    registerNatives(
      kTestClassJavaName,
      {
        makeNativeMethod("nativeVerifyLoomConstants", verifyLoomConstants),
        makeNativeMethod("nativeVerifyQuickLogLoomConstants", verifyQuickLogLoomConstants),
        makeNativeMethod("nativeVerifyQuickLogProcAndPerfConstants",
          verifyQuickLogProcAndPerfConstants)
      });
  });
}
