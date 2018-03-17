/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_EVENTS_INL_H_
#define ART_RUNTIME_OPENJDKJVMTI_EVENTS_INL_H_

#include <array>

#include "events.h"

#include "art_jvmti.h"

namespace openjdkjvmti {

static inline ArtJvmtiEvent GetArtJvmtiEvent(ArtJvmTiEnv* env, jvmtiEvent e) {
  if (UNLIKELY(e == JVMTI_EVENT_CLASS_FILE_LOAD_HOOK)) {
    if (env->capabilities.can_retransform_classes) {
      return ArtJvmtiEvent::kClassFileLoadHookRetransformable;
    } else {
      return ArtJvmtiEvent::kClassFileLoadHookNonRetransformable;
    }
  } else {
    return static_cast<ArtJvmtiEvent>(e);
  }
}

namespace impl {

// Infrastructure to achieve type safety for event dispatch.

#define FORALL_EVENT_TYPES(fn)                                                       \
  fn(VMInit,                  ArtJvmtiEvent::kVmInit)                                \
  fn(VMDeath,                 ArtJvmtiEvent::kVmDeath)                               \
  fn(ThreadStart,             ArtJvmtiEvent::kThreadStart)                           \
  fn(ThreadEnd,               ArtJvmtiEvent::kThreadEnd)                             \
  fn(ClassFileLoadHook,       ArtJvmtiEvent::kClassFileLoadHookRetransformable)      \
  fn(ClassFileLoadHook,       ArtJvmtiEvent::kClassFileLoadHookNonRetransformable)   \
  fn(ClassLoad,               ArtJvmtiEvent::kClassLoad)                             \
  fn(ClassPrepare,            ArtJvmtiEvent::kClassPrepare)                          \
  fn(VMStart,                 ArtJvmtiEvent::kVmStart)                               \
  fn(Exception,               ArtJvmtiEvent::kException)                             \
  fn(ExceptionCatch,          ArtJvmtiEvent::kExceptionCatch)                        \
  fn(SingleStep,              ArtJvmtiEvent::kSingleStep)                            \
  fn(FramePop,                ArtJvmtiEvent::kFramePop)                              \
  fn(Breakpoint,              ArtJvmtiEvent::kBreakpoint)                            \
  fn(FieldAccess,             ArtJvmtiEvent::kFieldAccess)                           \
  fn(FieldModification,       ArtJvmtiEvent::kFieldModification)                     \
  fn(MethodEntry,             ArtJvmtiEvent::kMethodEntry)                           \
  fn(MethodExit,              ArtJvmtiEvent::kMethodExit)                            \
  fn(NativeMethodBind,        ArtJvmtiEvent::kNativeMethodBind)                      \
  fn(CompiledMethodLoad,      ArtJvmtiEvent::kCompiledMethodLoad)                    \
  fn(CompiledMethodUnload,    ArtJvmtiEvent::kCompiledMethodUnload)                  \
  fn(DynamicCodeGenerated,    ArtJvmtiEvent::kDynamicCodeGenerated)                  \
  fn(DataDumpRequest,         ArtJvmtiEvent::kDataDumpRequest)                       \
  fn(MonitorWait,             ArtJvmtiEvent::kMonitorWait)                           \
  fn(MonitorWaited,           ArtJvmtiEvent::kMonitorWaited)                         \
  fn(MonitorContendedEnter,   ArtJvmtiEvent::kMonitorContendedEnter)                 \
  fn(MonitorContendedEntered, ArtJvmtiEvent::kMonitorContendedEntered)               \
  fn(ResourceExhausted,       ArtJvmtiEvent::kResourceExhausted)                     \
  fn(GarbageCollectionStart,  ArtJvmtiEvent::kGarbageCollectionStart)                \
  fn(GarbageCollectionFinish, ArtJvmtiEvent::kGarbageCollectionFinish)               \
  fn(ObjectFree,              ArtJvmtiEvent::kObjectFree)                            \
  fn(VMObjectAlloc,           ArtJvmtiEvent::kVmObjectAlloc)

template <ArtJvmtiEvent kEvent>
struct EventFnType {
};

#define EVENT_FN_TYPE(name, enum_name)               \
template <>                                          \
struct EventFnType<enum_name> {                      \
  using type = decltype(jvmtiEventCallbacks().name); \
};

FORALL_EVENT_TYPES(EVENT_FN_TYPE)

#undef EVENT_FN_TYPE

template <ArtJvmtiEvent kEvent>
ALWAYS_INLINE inline typename EventFnType<kEvent>::type GetCallback(ArtJvmTiEnv* env);

#define GET_CALLBACK(name, enum_name)                                     \
template <>                                                               \
ALWAYS_INLINE inline EventFnType<enum_name>::type GetCallback<enum_name>( \
    ArtJvmTiEnv* env) {                                                   \
  if (env->event_callbacks == nullptr) {                                  \
    return nullptr;                                                       \
  }                                                                       \
  return env->event_callbacks->name;                                      \
}

FORALL_EVENT_TYPES(GET_CALLBACK)

#undef GET_CALLBACK

#undef FORALL_EVENT_TYPES

}  // namespace impl

// C++ does not allow partial template function specialization. The dispatch for our separated
// ClassFileLoadHook event types is the same, so use this helper for code deduplication.
// TODO Locking of some type!
template <ArtJvmtiEvent kEvent>
inline void EventHandler::DispatchClassFileLoadHookEvent(art::Thread* thread,
                                                         JNIEnv* jnienv,
                                                         jclass class_being_redefined,
                                                         jobject loader,
                                                         const char* name,
                                                         jobject protection_domain,
                                                         jint class_data_len,
                                                         const unsigned char* class_data,
                                                         jint* new_class_data_len,
                                                         unsigned char** new_class_data) const {
  static_assert(kEvent == ArtJvmtiEvent::kClassFileLoadHookRetransformable ||
                kEvent == ArtJvmtiEvent::kClassFileLoadHookNonRetransformable, "Unsupported event");
  DCHECK(*new_class_data == nullptr);
  jint current_len = class_data_len;
  unsigned char* current_class_data = const_cast<unsigned char*>(class_data);
  ArtJvmTiEnv* last_env = nullptr;
  for (ArtJvmTiEnv* env : envs) {
    if (env == nullptr) {
      continue;
    }
    if (ShouldDispatch<kEvent>(env, thread)) {
      jint new_len = 0;
      unsigned char* new_data = nullptr;
      auto callback = impl::GetCallback<kEvent>(env);
      callback(env,
               jnienv,
               class_being_redefined,
               loader,
               name,
               protection_domain,
               current_len,
               current_class_data,
               &new_len,
               &new_data);
      if (new_data != nullptr && new_data != current_class_data) {
        // Destroy the data the last transformer made. We skip this if the previous state was the
        // initial one since we don't know here which jvmtiEnv allocated it.
        // NB Currently this doesn't matter since all allocations just go to malloc but in the
        // future we might have jvmtiEnv's keep track of their allocations for leak-checking.
        if (last_env != nullptr) {
          last_env->Deallocate(current_class_data);
        }
        last_env = env;
        current_class_data = new_data;
        current_len = new_len;
      }
    }
  }
  if (last_env != nullptr) {
    *new_class_data_len = current_len;
    *new_class_data = current_class_data;
  }
}

// Our goal for DispatchEvent: Do not allow implicit type conversion. Types of ...args must match
// exactly the argument types of the corresponding Jvmti kEvent function pointer.

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::DispatchEvent(art::Thread* thread, Args... args) const {
  for (ArtJvmTiEnv* env : envs) {
    if (env != nullptr) {
      DispatchEvent<kEvent, Args...>(env, thread, args...);
    }
  }
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline void EventHandler::DispatchEvent(ArtJvmTiEnv* env, art::Thread* thread, Args... args) const {
  using FnType = void(jvmtiEnv*, Args...);
  if (ShouldDispatch<kEvent>(env, thread)) {
    FnType* callback = impl::GetCallback<kEvent>(env);
    if (callback != nullptr) {
      (*callback)(env, args...);
    }
  }
}

// Need to give a custom specialization for NativeMethodBind since it has to deal with an out
// variable.
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kNativeMethodBind>(art::Thread* thread,
                                                                          JNIEnv* jnienv,
                                                                          jthread jni_thread,
                                                                          jmethodID method,
                                                                          void* cur_method,
                                                                          void** new_method) const {
  *new_method = cur_method;
  for (ArtJvmTiEnv* env : envs) {
    if (env != nullptr && ShouldDispatch<ArtJvmtiEvent::kNativeMethodBind>(env, thread)) {
      auto callback = impl::GetCallback<ArtJvmtiEvent::kNativeMethodBind>(env);
      (*callback)(env, jnienv, jni_thread, method, cur_method, new_method);
      if (*new_method != nullptr) {
        cur_method = *new_method;
      }
    }
  }
}

// C++ does not allow partial template function specialization. The dispatch for our separated
// ClassFileLoadHook event types is the same, and in the DispatchClassFileLoadHookEvent helper.
// The following two DispatchEvent specializations dispatch to it.
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}

template <ArtJvmtiEvent kEvent>
inline bool EventHandler::ShouldDispatch(ArtJvmTiEnv* env,
                                         art::Thread* thread) {
  bool dispatch = env->event_masks.global_event_mask.Test(kEvent);

  if (!dispatch && thread != nullptr && env->event_masks.unioned_thread_event_mask.Test(kEvent)) {
    EventMask* mask = env->event_masks.GetEventMaskOrNull(thread);
    dispatch = mask != nullptr && mask->Test(kEvent);
  }
  return dispatch;
}

inline void EventHandler::RecalculateGlobalEventMask(ArtJvmtiEvent event) {
  bool union_value = false;
  for (const ArtJvmTiEnv* stored_env : envs) {
    if (stored_env == nullptr) {
      continue;
    }
    union_value |= stored_env->event_masks.global_event_mask.Test(event);
    union_value |= stored_env->event_masks.unioned_thread_event_mask.Test(event);
    if (union_value) {
      break;
    }
  }
  global_mask.Set(event, union_value);
}

inline bool EventHandler::NeedsEventUpdate(ArtJvmTiEnv* env,
                                           const jvmtiCapabilities& caps,
                                           bool added) {
  ArtJvmtiEvent event = added ? ArtJvmtiEvent::kClassFileLoadHookNonRetransformable
                              : ArtJvmtiEvent::kClassFileLoadHookRetransformable;
  return caps.can_retransform_classes == 1 &&
      IsEventEnabledAnywhere(event) &&
      env->event_masks.IsEnabledAnywhere(event);
}

inline void EventHandler::HandleChangedCapabilities(ArtJvmTiEnv* env,
                                                    const jvmtiCapabilities& caps,
                                                    bool added) {
  if (UNLIKELY(NeedsEventUpdate(env, caps, added))) {
    env->event_masks.HandleChangedCapabilities(caps, added);
    if (caps.can_retransform_classes == 1) {
      RecalculateGlobalEventMask(ArtJvmtiEvent::kClassFileLoadHookRetransformable);
      RecalculateGlobalEventMask(ArtJvmtiEvent::kClassFileLoadHookNonRetransformable);
    }
  }
}

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_EVENTS_INL_H_
