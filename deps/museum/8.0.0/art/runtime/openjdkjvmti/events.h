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

#ifndef ART_RUNTIME_OPENJDKJVMTI_EVENTS_H_
#define ART_RUNTIME_OPENJDKJVMTI_EVENTS_H_

#include <bitset>
#include <vector>

#include "base/logging.h"
#include "jvmti.h"
#include "thread.h"

namespace openjdkjvmti {

struct ArtJvmTiEnv;
class JvmtiAllocationListener;
class JvmtiGcPauseListener;

// an enum for ArtEvents. This differs from the JVMTI events only in that we distinguish between
// retransformation capable and incapable loading
enum class ArtJvmtiEvent {
    kMinEventTypeVal = JVMTI_MIN_EVENT_TYPE_VAL,
    kVmInit = JVMTI_EVENT_VM_INIT,
    kVmDeath = JVMTI_EVENT_VM_DEATH,
    kThreadStart = JVMTI_EVENT_THREAD_START,
    kThreadEnd = JVMTI_EVENT_THREAD_END,
    kClassFileLoadHookNonRetransformable = JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
    kClassLoad = JVMTI_EVENT_CLASS_LOAD,
    kClassPrepare = JVMTI_EVENT_CLASS_PREPARE,
    kVmStart = JVMTI_EVENT_VM_START,
    kException = JVMTI_EVENT_EXCEPTION,
    kExceptionCatch = JVMTI_EVENT_EXCEPTION_CATCH,
    kSingleStep = JVMTI_EVENT_SINGLE_STEP,
    kFramePop = JVMTI_EVENT_FRAME_POP,
    kBreakpoint = JVMTI_EVENT_BREAKPOINT,
    kFieldAccess = JVMTI_EVENT_FIELD_ACCESS,
    kFieldModification = JVMTI_EVENT_FIELD_MODIFICATION,
    kMethodEntry = JVMTI_EVENT_METHOD_ENTRY,
    kMethodExit = JVMTI_EVENT_METHOD_EXIT,
    kNativeMethodBind = JVMTI_EVENT_NATIVE_METHOD_BIND,
    kCompiledMethodLoad = JVMTI_EVENT_COMPILED_METHOD_LOAD,
    kCompiledMethodUnload = JVMTI_EVENT_COMPILED_METHOD_UNLOAD,
    kDynamicCodeGenerated = JVMTI_EVENT_DYNAMIC_CODE_GENERATED,
    kDataDumpRequest = JVMTI_EVENT_DATA_DUMP_REQUEST,
    kMonitorWait = JVMTI_EVENT_MONITOR_WAIT,
    kMonitorWaited = JVMTI_EVENT_MONITOR_WAITED,
    kMonitorContendedEnter = JVMTI_EVENT_MONITOR_CONTENDED_ENTER,
    kMonitorContendedEntered = JVMTI_EVENT_MONITOR_CONTENDED_ENTERED,
    kResourceExhausted = JVMTI_EVENT_RESOURCE_EXHAUSTED,
    kGarbageCollectionStart = JVMTI_EVENT_GARBAGE_COLLECTION_START,
    kGarbageCollectionFinish = JVMTI_EVENT_GARBAGE_COLLECTION_FINISH,
    kObjectFree = JVMTI_EVENT_OBJECT_FREE,
    kVmObjectAlloc = JVMTI_EVENT_VM_OBJECT_ALLOC,
    kClassFileLoadHookRetransformable = JVMTI_MAX_EVENT_TYPE_VAL + 1,
    kMaxEventTypeVal = kClassFileLoadHookRetransformable,
};

// Convert a jvmtiEvent into a ArtJvmtiEvent
ALWAYS_INLINE static inline ArtJvmtiEvent GetArtJvmtiEvent(ArtJvmTiEnv* env, jvmtiEvent e);

static inline jvmtiEvent GetJvmtiEvent(ArtJvmtiEvent e) {
  if (UNLIKELY(e == ArtJvmtiEvent::kClassFileLoadHookRetransformable)) {
    return JVMTI_EVENT_CLASS_FILE_LOAD_HOOK;
  } else {
    return static_cast<jvmtiEvent>(e);
  }
}

struct EventMask {
  static constexpr size_t kEventsSize =
      static_cast<size_t>(ArtJvmtiEvent::kMaxEventTypeVal) -
      static_cast<size_t>(ArtJvmtiEvent::kMinEventTypeVal) + 1;
  std::bitset<kEventsSize> bit_set;

  static bool EventIsInRange(ArtJvmtiEvent event) {
    return event >= ArtJvmtiEvent::kMinEventTypeVal && event <= ArtJvmtiEvent::kMaxEventTypeVal;
  }

  void Set(ArtJvmtiEvent event, bool value = true) {
    DCHECK(EventIsInRange(event));
    bit_set.set(static_cast<size_t>(event) - static_cast<size_t>(ArtJvmtiEvent::kMinEventTypeVal),
                value);
  }

  bool Test(ArtJvmtiEvent event) const {
    DCHECK(EventIsInRange(event));
    return bit_set.test(
        static_cast<size_t>(event) - static_cast<size_t>(ArtJvmtiEvent::kMinEventTypeVal));
  }
};

struct EventMasks {
  // The globally enabled events.
  EventMask global_event_mask;

  // The per-thread enabled events.

  // It is not enough to store a Thread pointer, as these may be reused. Use the pointer and the
  // thread id.
  // Note: We could just use the tid like tracing does.
  using UniqueThread = std::pair<art::Thread*, uint32_t>;
  // TODO: Native thread objects are immovable, so we can use them as keys in an (unordered) map,
  //       if necessary.
  std::vector<std::pair<UniqueThread, EventMask>> thread_event_masks;

  // A union of the per-thread events, for fast-pathing.
  EventMask unioned_thread_event_mask;

  EventMask& GetEventMask(art::Thread* thread);
  EventMask* GetEventMaskOrNull(art::Thread* thread);
  void EnableEvent(art::Thread* thread, ArtJvmtiEvent event);
  void DisableEvent(art::Thread* thread, ArtJvmtiEvent event);
  bool IsEnabledAnywhere(ArtJvmtiEvent event);
  // Make any changes to event masks needed for the given capability changes. If caps_added is true
  // then caps is all the newly set capabilities of the jvmtiEnv. If it is false then caps is the
  // set of all capabilities that were removed from the jvmtiEnv.
  void HandleChangedCapabilities(const jvmtiCapabilities& caps, bool caps_added);
};

// Helper class for event handling.
class EventHandler {
 public:
  EventHandler();
  ~EventHandler();

  // Register an env. It is assumed that this happens on env creation, that is, no events are
  // enabled, yet.
  void RegisterArtJvmTiEnv(ArtJvmTiEnv* env);

  // Remove an env.
  void RemoveArtJvmTiEnv(ArtJvmTiEnv* env);

  bool IsEventEnabledAnywhere(ArtJvmtiEvent event) const {
    if (!EventMask::EventIsInRange(event)) {
      return false;
    }
    return global_mask.Test(event);
  }

  jvmtiError SetEvent(ArtJvmTiEnv* env,
                      art::Thread* thread,
                      ArtJvmtiEvent event,
                      jvmtiEventMode mode);

  // Dispatch event to all registered environments.
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline void DispatchEvent(art::Thread* thread, Args... args) const;
  // Dispatch event to the given environment, only.
  template <ArtJvmtiEvent kEvent, typename ...Args>
  ALWAYS_INLINE
  inline void DispatchEvent(ArtJvmTiEnv* env, art::Thread* thread, Args... args) const;

  // Tell the event handler capabilities were added/lost so it can adjust the sent events.If
  // caps_added is true then caps is all the newly set capabilities of the jvmtiEnv. If it is false
  // then caps is the set of all capabilities that were removed from the jvmtiEnv.
  ALWAYS_INLINE
  inline void HandleChangedCapabilities(ArtJvmTiEnv* env,
                                        const jvmtiCapabilities& caps,
                                        bool added);

 private:
  template <ArtJvmtiEvent kEvent>
  ALWAYS_INLINE
  static inline bool ShouldDispatch(ArtJvmTiEnv* env, art::Thread* thread);

  ALWAYS_INLINE
  inline bool NeedsEventUpdate(ArtJvmTiEnv* env,
                               const jvmtiCapabilities& caps,
                               bool added);

  // Recalculates the event mask for the given event.
  ALWAYS_INLINE
  inline void RecalculateGlobalEventMask(ArtJvmtiEvent event);

  template <ArtJvmtiEvent kEvent>
  ALWAYS_INLINE inline void DispatchClassFileLoadHookEvent(art::Thread* thread,
                                                           JNIEnv* jnienv,
                                                           jclass class_being_redefined,
                                                           jobject loader,
                                                           const char* name,
                                                           jobject protection_domain,
                                                           jint class_data_len,
                                                           const unsigned char* class_data,
                                                           jint* new_class_data_len,
                                                           unsigned char** new_class_data) const;

  void HandleEventType(ArtJvmtiEvent event, bool enable);

  // List of all JvmTiEnv objects that have been created, in their creation order.
  // NB Some elements might be null representing envs that have been deleted. They should be skipped
  // anytime this list is used.
  std::vector<ArtJvmTiEnv*> envs;

  // A union of all enabled events, anywhere.
  EventMask global_mask;

  std::unique_ptr<JvmtiAllocationListener> alloc_listener_;
  std::unique_ptr<JvmtiGcPauseListener> gc_pause_listener_;
};

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_EVENTS_H_
