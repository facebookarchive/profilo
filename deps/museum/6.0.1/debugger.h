/*
 * Copyright (C) 2008 The Android Open Source Project
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

/*
 * Dalvik-specific side of debugger support.  (The JDWP code is intended to
 * be relatively generic.)
 */
#ifndef ART_RUNTIME_DEBUGGER_H_
#define ART_RUNTIME_DEBUGGER_H_

#include <pthread.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "gc_root.h"
#include "jdwp/jdwp.h"
#include "jni.h"
#include "jvalue.h"
#include "object_callbacks.h"
#include "thread_state.h"

namespace art {
namespace mirror {
class Class;
class Object;
class Throwable;
}  // namespace mirror
class AllocRecord;
class ArtField;
class ArtMethod;
class ObjectRegistry;
class ScopedObjectAccess;
class ScopedObjectAccessUnchecked;
class StackVisitor;
class Thread;

/*
 * Invoke-during-breakpoint support.
 */
struct DebugInvokeReq {
  DebugInvokeReq(uint32_t invoke_request_id, JDWP::ObjectId invoke_thread_id,
                 mirror::Object* invoke_receiver, mirror::Class* invoke_class,
                 ArtMethod* invoke_method, uint32_t invoke_options,
                 uint64_t args[], uint32_t args_count)
      : request_id(invoke_request_id), thread_id(invoke_thread_id), receiver(invoke_receiver),
        klass(invoke_class), method(invoke_method), arg_count(args_count), arg_values(args),
        options(invoke_options), reply(JDWP::expandBufAlloc()) {
  }

  ~DebugInvokeReq() {
    JDWP::expandBufFree(reply);
  }

  // Request
  const uint32_t request_id;
  const JDWP::ObjectId thread_id;
  GcRoot<mirror::Object> receiver;      // not used for ClassType.InvokeMethod.
  GcRoot<mirror::Class> klass;
  ArtMethod* const method;
  const uint32_t arg_count;
  std::unique_ptr<uint64_t[]> arg_values;   // will be null if arg_count_ == 0. We take ownership
                                            // of this array so we must delete it upon destruction.
  const uint32_t options;

  // Reply
  JDWP::ExpandBuf* const reply;

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugInvokeReq);
};

// Thread local data-structure that holds fields for controlling single-stepping.
class SingleStepControl {
 public:
  SingleStepControl(JDWP::JdwpStepSize step_size, JDWP::JdwpStepDepth step_depth,
                    int stack_depth, ArtMethod* method)
      : step_size_(step_size), step_depth_(step_depth),
        stack_depth_(stack_depth), method_(method) {
  }

  JDWP::JdwpStepSize GetStepSize() const {
    return step_size_;
  }

  JDWP::JdwpStepDepth GetStepDepth() const {
    return step_depth_;
  }

  int GetStackDepth() const {
    return stack_depth_;
  }

  ArtMethod* GetMethod() const {
    return method_;
  }

  const std::set<uint32_t>& GetDexPcs() const {
    return dex_pcs_;
  }

  void AddDexPc(uint32_t dex_pc);

  bool ContainsDexPc(uint32_t dex_pc) const;

 private:
  // See JdwpStepSize and JdwpStepDepth for details.
  const JDWP::JdwpStepSize step_size_;
  const JDWP::JdwpStepDepth step_depth_;

  // The stack depth when this single-step was initiated. This is used to support SD_OVER and SD_OUT
  // single-step depth.
  const int stack_depth_;

  // The location this single-step was initiated from.
  // A single-step is initiated in a suspended thread. We save here the current method and the
  // set of DEX pcs associated to the source line number where the suspension occurred.
  // This is used to support SD_INTO and SD_OVER single-step depths so we detect when a single-step
  // causes the execution of an instruction in a different method or at a different line number.
  ArtMethod* method_;

  std::set<uint32_t> dex_pcs_;

  DISALLOW_COPY_AND_ASSIGN(SingleStepControl);
};

// TODO rename to InstrumentationRequest.
class DeoptimizationRequest {
 public:
  enum Kind {
    kNothing,                   // no action.
    kRegisterForEvent,          // start listening for instrumentation event.
    kUnregisterForEvent,        // stop listening for instrumentation event.
    kFullDeoptimization,        // deoptimize everything.
    kFullUndeoptimization,      // undeoptimize everything.
    kSelectiveDeoptimization,   // deoptimize one method.
    kSelectiveUndeoptimization  // undeoptimize one method.
  };

  DeoptimizationRequest() : kind_(kNothing), instrumentation_event_(0), method_(nullptr) {}

  DeoptimizationRequest(const DeoptimizationRequest& other)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : kind_(other.kind_), instrumentation_event_(other.instrumentation_event_) {
    // Create a new JNI global reference for the method.
    SetMethod(other.Method());
  }

  ArtMethod* Method() const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  void SetMethod(ArtMethod* m) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Name 'Kind()' would collide with the above enum name.
  Kind GetKind() const {
    return kind_;
  }

  void SetKind(Kind kind) {
    kind_ = kind;
  }

  uint32_t InstrumentationEvent() const {
    return instrumentation_event_;
  }

  void SetInstrumentationEvent(uint32_t instrumentation_event) {
    instrumentation_event_ = instrumentation_event;
  }

 private:
  Kind kind_;

  // TODO we could use a union to hold the instrumentation_event and the method since they
  // respectively have sense only for kRegisterForEvent/kUnregisterForEvent and
  // kSelectiveDeoptimization/kSelectiveUndeoptimization.

  // Event to start or stop listening to. Only for kRegisterForEvent and kUnregisterForEvent.
  uint32_t instrumentation_event_;

  // Method for selective deoptimization.
  jmethodID method_;
};
std::ostream& operator<<(std::ostream& os, const DeoptimizationRequest::Kind& rhs);

class Dbg {
 public:
  class TypeCache {
   public:
    // Returns a weak global for the input type. Deduplicates.
    jobject Add(mirror::Class* t) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_,
                                                        Locks::alloc_tracker_lock_);
    // Clears the type cache and deletes all the weak global refs.
    void Clear() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_,
                                       Locks::alloc_tracker_lock_);

   private:
    std::multimap<int32_t, jobject> objects_;
  };

  static void SetJdwpAllowed(bool allowed);

  static void StartJdwp();
  static void StopJdwp();

  // Invoked by the GC in case we need to keep DDMS informed.
  static void GcDidFinish() LOCKS_EXCLUDED(Locks::mutator_lock_);

  // Return the DebugInvokeReq for the current thread.
  static DebugInvokeReq* GetInvokeReq();

  static Thread* GetDebugThread();
  static void ClearWaitForEventThread();

  /*
   * Enable/disable breakpoints and step modes.  Used to provide a heads-up
   * when the debugger attaches.
   */
  static void Connected();
  static void GoActive()
      LOCKS_EXCLUDED(Locks::breakpoint_lock_, Locks::deoptimization_lock_, Locks::mutator_lock_);
  static void Disconnected() LOCKS_EXCLUDED(Locks::deoptimization_lock_, Locks::mutator_lock_);
  static void Dispose() {
    gDisposed = true;
  }

  // Returns true if we're actually debugging with a real debugger, false if it's
  // just DDMS (or nothing at all).
  static bool IsDebuggerActive() {
    return gDebuggerActive;
  }

  // Configures JDWP with parsed command-line options.
  static void ConfigureJdwp(const JDWP::JdwpOptions& jdwp_options);

  // Returns true if we had -Xrunjdwp or -agentlib:jdwp= on the command line.
  static bool IsJdwpConfigured();

  // Returns true if a method has any breakpoints.
  static bool MethodHasAnyBreakpoints(ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_);

  static bool IsDisposed() {
    return gDisposed;
  }

  /*
   * Time, in milliseconds, since the last debugger activity.  Does not
   * include DDMS activity.  Returns -1 if there has been no activity.
   * Returns 0 if we're in the middle of handling a debugger request.
   */
  static int64_t LastDebuggerActivity();

  static void UndoDebuggerSuspensions()
    LOCKS_EXCLUDED(Locks::thread_list_lock_,
                   Locks::thread_suspend_count_lock_);

  /*
   * Class, Object, Array
   */
  static std::string GetClassName(JDWP::RefTypeId id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static std::string GetClassName(mirror::Class* klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetClassObject(JDWP::RefTypeId id, JDWP::ObjectId* class_object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetSuperclass(JDWP::RefTypeId id, JDWP::RefTypeId* superclass_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetClassLoader(JDWP::RefTypeId id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetModifiers(JDWP::RefTypeId id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetReflectedType(JDWP::RefTypeId class_id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void GetClassList(std::vector<JDWP::RefTypeId>* classes)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetClassInfo(JDWP::RefTypeId class_id, JDWP::JdwpTypeTag* pTypeTag,
                                      uint32_t* pStatus, std::string* pDescriptor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void FindLoadedClassBySignature(const char* descriptor, std::vector<JDWP::RefTypeId>* ids)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetReferenceType(JDWP::ObjectId object_id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetSignature(JDWP::RefTypeId ref_type_id, std::string* signature)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetSourceFile(JDWP::RefTypeId ref_type_id, std::string* source_file)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetObjectTag(JDWP::ObjectId object_id, uint8_t* tag)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static size_t GetTagWidth(JDWP::JdwpTag tag);

  static JDWP::JdwpError GetArrayLength(JDWP::ObjectId array_id, int32_t* length)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputArray(JDWP::ObjectId array_id, int offset, int count,
                                     JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetArrayElements(JDWP::ObjectId array_id, int offset, int count,
                                          JDWP::Request* request)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpError CreateString(const std::string& str, JDWP::ObjectId* new_string_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError CreateObject(JDWP::RefTypeId class_id, JDWP::ObjectId* new_object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError CreateArrayObject(JDWP::RefTypeId array_class_id, uint32_t length,
                                           JDWP::ObjectId* new_array_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Event filtering.
  //
  static bool MatchThread(JDWP::ObjectId expected_thread_id, Thread* event_thread)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool MatchLocation(const JDWP::JdwpLocation& expected_location,
                            const JDWP::EventLocation& event_location)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool MatchType(mirror::Class* event_class, JDWP::RefTypeId class_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool MatchField(JDWP::RefTypeId expected_type_id, JDWP::FieldId expected_field_id,
                         ArtField* event_field)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool MatchInstance(JDWP::ObjectId expected_instance_id, mirror::Object* event_instance)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Monitors.
  //
  static JDWP::JdwpError GetMonitorInfo(JDWP::ObjectId object_id, JDWP::ExpandBuf* reply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetOwnedMonitors(JDWP::ObjectId thread_id,
                                          std::vector<JDWP::ObjectId>* monitors,
                                          std::vector<uint32_t>* stack_depths)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetContendedMonitor(JDWP::ObjectId thread_id,
                                             JDWP::ObjectId* contended_monitor)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Heap.
  //
  static JDWP::JdwpError GetInstanceCounts(const std::vector<JDWP::RefTypeId>& class_ids,
                                           std::vector<uint64_t>* counts)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetInstances(JDWP::RefTypeId class_id, int32_t max_count,
                                      std::vector<JDWP::ObjectId>* instances)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetReferringObjects(JDWP::ObjectId object_id, int32_t max_count,
                                             std::vector<JDWP::ObjectId>* referring_objects)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError DisableCollection(JDWP::ObjectId object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError EnableCollection(JDWP::ObjectId object_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError IsCollected(JDWP::ObjectId object_id, bool* is_collected)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DisposeObject(JDWP::ObjectId object_id, uint32_t reference_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  //
  // Methods and fields.
  //
  static std::string GetMethodName(JDWP::MethodId method_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputDeclaredFields(JDWP::RefTypeId ref_type_id, bool with_generic,
                                              JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputDeclaredMethods(JDWP::RefTypeId ref_type_id, bool with_generic,
                                               JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError OutputDeclaredInterfaces(JDWP::RefTypeId ref_type_id,
                                                  JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputLineTable(JDWP::RefTypeId ref_type_id, JDWP::MethodId method_id,
                              JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputVariableTable(JDWP::RefTypeId ref_type_id, JDWP::MethodId id, bool with_generic,
                                  JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputMethodReturnValue(JDWP::MethodId method_id, const JValue* return_value,
                                      JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputFieldValue(JDWP::FieldId field_id, const JValue* field_value,
                               JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetBytecodes(JDWP::RefTypeId class_id, JDWP::MethodId method_id,
                                      std::vector<uint8_t>* bytecodes)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static std::string GetFieldName(JDWP::FieldId field_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpTag GetFieldBasicTag(JDWP::FieldId field_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpTag GetStaticFieldBasicTag(JDWP::FieldId field_id)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetFieldValue(JDWP::ObjectId object_id, JDWP::FieldId field_id,
                                       JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetFieldValue(JDWP::ObjectId object_id, JDWP::FieldId field_id,
                                       uint64_t value, int width)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetStaticFieldValue(JDWP::RefTypeId ref_type_id, JDWP::FieldId field_id,
                                             JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetStaticFieldValue(JDWP::FieldId field_id, uint64_t value, int width)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpError StringToUtf8(JDWP::ObjectId string_id, std::string* str)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void OutputJValue(JDWP::JdwpTag tag, const JValue* return_value, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Thread, ThreadGroup, Frame
   */
  static JDWP::JdwpError GetThreadName(JDWP::ObjectId thread_id, std::string* name)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);
  static JDWP::JdwpError GetThreadGroup(JDWP::ObjectId thread_id, JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);
  static JDWP::JdwpError GetThreadGroupName(JDWP::ObjectId thread_group_id,
                                            JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetThreadGroupParent(JDWP::ObjectId thread_group_id,
                                              JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetThreadGroupChildren(JDWP::ObjectId thread_group_id,
                                                JDWP::ExpandBuf* pReply)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::ObjectId GetSystemThreadGroupId()
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpThreadStatus ToJdwpThreadStatus(ThreadState state);
  static JDWP::JdwpError GetThreadStatus(JDWP::ObjectId thread_id,
                                         JDWP::JdwpThreadStatus* pThreadStatus,
                                         JDWP::JdwpSuspendStatus* pSuspendStatus)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);
  static JDWP::JdwpError GetThreadDebugSuspendCount(JDWP::ObjectId thread_id,
                                                    JDWP::ExpandBuf* pReply)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);
  // static void WaitForSuspend(JDWP::ObjectId thread_id);

  // Fills 'thread_ids' with the threads in the given thread group. If thread_group_id == 0,
  // returns all threads.
  static void GetThreads(mirror::Object* thread_group, std::vector<JDWP::ObjectId>* thread_ids)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpError GetThreadFrameCount(JDWP::ObjectId thread_id, size_t* result)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);
  static JDWP::JdwpError GetThreadFrames(JDWP::ObjectId thread_id, size_t start_frame,
                                         size_t frame_count, JDWP::ExpandBuf* buf)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::ObjectId GetThreadSelfId() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::ObjectId GetThreadId(Thread* thread) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void SuspendVM()
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);
  static void ResumeVM()
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);
  static JDWP::JdwpError SuspendThread(JDWP::ObjectId thread_id, bool request_suspension = true)
      LOCKS_EXCLUDED(Locks::mutator_lock_,
                     Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_);

  static void ResumeThread(JDWP::ObjectId thread_id)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void SuspendSelf();

  static JDWP::JdwpError GetThisObject(JDWP::ObjectId thread_id, JDWP::FrameId frame_id,
                                       JDWP::ObjectId* result)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError GetLocalValues(JDWP::Request* request, JDWP::ExpandBuf* pReply)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetLocalValues(JDWP::Request* request)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpError Interrupt(JDWP::ObjectId thread_id)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);

  /*
   * Debugger notification
   */
  enum EventFlag {
    kBreakpoint     = 0x01,
    kSingleStep     = 0x02,
    kMethodEntry    = 0x04,
    kMethodExit     = 0x08,
  };
  static void PostFieldAccessEvent(ArtMethod* m, int dex_pc, mirror::Object* this_object,
                                   ArtField* f)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostFieldModificationEvent(ArtMethod* m, int dex_pc,
                                         mirror::Object* this_object, ArtField* f,
                                         const JValue* field_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostException(mirror::Throwable* exception)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostThreadStart(Thread* t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostThreadDeath(Thread* t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostClassPrepare(mirror::Class* c)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void UpdateDebugger(Thread* thread, mirror::Object* this_object,
                             ArtMethod* method, uint32_t new_dex_pc,
                             int event_flags, const JValue* return_value)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Indicates whether we need deoptimization for debugging.
  static bool RequiresDeoptimization();

  // Records deoptimization request in the queue.
  static void RequestDeoptimization(const DeoptimizationRequest& req)
      LOCKS_EXCLUDED(Locks::deoptimization_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Manage deoptimization after updating JDWP events list. Suspends all threads, processes each
  // request and finally resumes all threads.
  static void ManageDeoptimization()
      LOCKS_EXCLUDED(Locks::deoptimization_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Breakpoints.
  static void WatchLocation(const JDWP::JdwpLocation* pLoc, DeoptimizationRequest* req)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void UnwatchLocation(const JDWP::JdwpLocation* pLoc, DeoptimizationRequest* req)
      LOCKS_EXCLUDED(Locks::breakpoint_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Forced interpreter checkers for single-step and continue support.
   */

  // Indicates whether we need to force the use of interpreter to invoke a method.
  // This allows to single-step or continue into the called method.
  static bool IsForcedInterpreterNeededForCalling(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsDebuggerActive()) {
      return false;
    }
    return IsForcedInterpreterNeededForCallingImpl(thread, m);
  }

  // Indicates whether we need to force the use of interpreter entrypoint when calling a
  // method through the resolution trampoline. This allows to single-step or continue into
  // the called method.
  static bool IsForcedInterpreterNeededForResolution(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsDebuggerActive()) {
      return false;
    }
    return IsForcedInterpreterNeededForResolutionImpl(thread, m);
  }

  // Indicates whether we need to force the use of instrumentation entrypoint when calling
  // a method through the resolution trampoline. This allows to deoptimize the stack for
  // debugging when we returned from the called method.
  static bool IsForcedInstrumentationNeededForResolution(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsDebuggerActive()) {
      return false;
    }
    return IsForcedInstrumentationNeededForResolutionImpl(thread, m);
  }

  // Indicates whether we need to force the use of interpreter when returning from the
  // interpreter into the runtime. This allows to deoptimize the stack and continue
  // execution with interpreter for debugging.
  static bool IsForcedInterpreterNeededForUpcall(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    if (!IsDebuggerActive()) {
      return false;
    }
    return IsForcedInterpreterNeededForUpcallImpl(thread, m);
  }

  // Single-stepping.
  static JDWP::JdwpError ConfigureStep(JDWP::ObjectId thread_id, JDWP::JdwpStepSize size,
                                       JDWP::JdwpStepDepth depth)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void UnconfigureStep(JDWP::ObjectId thread_id)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Invoke support
   */

  // Called by the JDWP thread to prepare invocation in the event thread (suspended on an event).
  // If the information sent by the debugger is incorrect, it will send a reply with the
  // appropriate error code. Otherwise, it will attach a DebugInvokeReq object to the event thread
  // and resume it (and possibly other threads depending on the invoke options).
  // Unlike other commands, the JDWP thread will not send the reply to the debugger (see
  // JdwpState::ProcessRequest). The reply will be sent by the event thread itself after method
  // invocation completes (see FinishInvokeMethod). This is required to allow the JDWP thread to
  // process incoming commands from the debugger while the invocation is still in progress in the
  // event thread, especially if it gets suspended by a debug event occurring in another thread.
  static JDWP::JdwpError PrepareInvokeMethod(uint32_t request_id, JDWP::ObjectId thread_id,
                                             JDWP::ObjectId object_id, JDWP::RefTypeId class_id,
                                             JDWP::MethodId method_id, uint32_t arg_count,
                                             uint64_t arg_values[], JDWP::JdwpTag* arg_types,
                                             uint32_t options)
      LOCKS_EXCLUDED(Locks::thread_list_lock_,
                     Locks::thread_suspend_count_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  // Called by the event thread to execute a method prepared by the JDWP thread in the given
  // DebugInvokeReq object. Once the invocation completes, the event thread attaches a reply
  // to that DebugInvokeReq object so it can be sent to the debugger only when the event thread
  // is ready to suspend (see FinishInvokeMethod).
  static void ExecuteMethod(DebugInvokeReq* pReq);

  // Called by the event thread to send the reply of the invoke (created in ExecuteMethod)
  // before suspending itself. This is to ensure the thread is ready to suspend before the
  // debugger receives the reply.
  static void FinishInvokeMethod(DebugInvokeReq* pReq);

  /*
   * DDM support.
   */
  static void DdmSendThreadNotification(Thread* t, uint32_t type)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSetThreadNotification(bool enable)
      LOCKS_EXCLUDED(Locks::thread_list_lock_);
  static bool DdmHandlePacket(JDWP::Request* request, uint8_t** pReplyBuf, int* pReplyLen);
  static void DdmConnected() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmDisconnected() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendChunk(uint32_t type, const std::vector<uint8_t>& bytes)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendChunk(uint32_t type, size_t len, const uint8_t* buf)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendChunkV(uint32_t type, const iovec* iov, int iov_count)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void VisitRoots(RootVisitor* visitor)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  /*
   * Recent allocation tracking support.
   */
  static void RecordAllocation(Thread* self, mirror::Class* type, size_t byte_count)
      LOCKS_EXCLUDED(Locks::alloc_tracker_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void SetAllocTrackingEnabled(bool enabled) LOCKS_EXCLUDED(Locks::alloc_tracker_lock_);
  static bool IsAllocTrackingEnabled() {
    return recent_allocation_records_ != nullptr;
  }
  static jbyteArray GetRecentAllocations()
      LOCKS_EXCLUDED(Locks::alloc_tracker_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static size_t HeadIndex() EXCLUSIVE_LOCKS_REQUIRED(Locks::alloc_tracker_lock_);
  static void DumpRecentAllocations() LOCKS_EXCLUDED(Locks::alloc_tracker_lock_);

  enum HpifWhen {
    HPIF_WHEN_NEVER = 0,
    HPIF_WHEN_NOW = 1,
    HPIF_WHEN_NEXT_GC = 2,
    HPIF_WHEN_EVERY_GC = 3
  };
  static int DdmHandleHpifChunk(HpifWhen when)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  enum HpsgWhen {
    HPSG_WHEN_NEVER = 0,
    HPSG_WHEN_EVERY_GC = 1,
  };
  enum HpsgWhat {
    HPSG_WHAT_MERGED_OBJECTS = 0,
    HPSG_WHAT_DISTINCT_OBJECTS = 1,
  };
  static bool DdmHandleHpsgNhsgChunk(HpsgWhen when, HpsgWhat what, bool native);

  static void DdmSendHeapInfo(HpifWhen reason)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void DdmSendHeapSegments(bool native)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static ObjectRegistry* GetObjectRegistry() {
    return gRegistry;
  }

  static JDWP::JdwpTag TagFromObject(const ScopedObjectAccessUnchecked& soa, mirror::Object* o)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpTypeTag GetTypeTag(mirror::Class* klass)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::FieldId ToFieldId(const ArtField* f)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void SetJdwpLocation(JDWP::JdwpLocation* location, ArtMethod* m, uint32_t dex_pc)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpState* GetJdwpState();

  static uint32_t GetInstrumentationEvents() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    return instrumentation_events_;
  }

 private:
  static void ExecuteMethodWithoutPendingException(ScopedObjectAccess& soa, DebugInvokeReq* pReq)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void BuildInvokeReply(JDWP::ExpandBuf* pReply, uint32_t request_id,
                               JDWP::JdwpTag result_tag, uint64_t result_value,
                               JDWP::ObjectId exception)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static JDWP::JdwpError GetLocalValue(const StackVisitor& visitor,
                                       ScopedObjectAccessUnchecked& soa, int slot,
                                       JDWP::JdwpTag tag, uint8_t* buf, size_t width)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static JDWP::JdwpError SetLocalValue(StackVisitor& visitor, int slot, JDWP::JdwpTag tag,
                                       uint64_t value, size_t width)
      LOCKS_EXCLUDED(Locks::thread_list_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void DdmBroadcast(bool connect) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  static void PostThreadStartOrStop(Thread*, uint32_t)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void PostLocationEvent(ArtMethod* method, int pcOffset,
                                mirror::Object* thisPtr, int eventFlags,
                                const JValue* return_value)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void ProcessDeoptimizationRequest(const DeoptimizationRequest& request)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::mutator_lock_);

  static void RequestDeoptimizationLocked(const DeoptimizationRequest& req)
      EXCLUSIVE_LOCKS_REQUIRED(Locks::deoptimization_lock_)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsForcedInterpreterNeededForCallingImpl(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsForcedInterpreterNeededForResolutionImpl(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsForcedInstrumentationNeededForResolutionImpl(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static bool IsForcedInterpreterNeededForUpcallImpl(Thread* thread, ArtMethod* m)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  static AllocRecord* recent_allocation_records_ PT_GUARDED_BY(Locks::alloc_tracker_lock_);
  static size_t alloc_record_max_ GUARDED_BY(Locks::alloc_tracker_lock_);
  static size_t alloc_record_head_ GUARDED_BY(Locks::alloc_tracker_lock_);
  static size_t alloc_record_count_ GUARDED_BY(Locks::alloc_tracker_lock_);

  // Indicates whether the debugger is making requests.
  static bool gDebuggerActive;

  // Indicates whether we should drop the JDWP connection because the runtime stops or the
  // debugger called VirtualMachine.Dispose.
  static bool gDisposed;

  // The registry mapping objects to JDWP ids.
  static ObjectRegistry* gRegistry;

  // Deoptimization requests to be processed each time the event list is updated. This is used when
  // registering and unregistering events so we do not deoptimize while holding the event list
  // lock.
  // TODO rename to instrumentation_requests.
  static std::vector<DeoptimizationRequest> deoptimization_requests_ GUARDED_BY(Locks::deoptimization_lock_);

  // Count the number of events requiring full deoptimization. When the counter is > 0, everything
  // is deoptimized, otherwise everything is undeoptimized.
  // Note: we fully deoptimize on the first event only (when the counter is set to 1). We fully
  // undeoptimize when the last event is unregistered (when the counter is set to 0).
  static size_t full_deoptimization_event_count_ GUARDED_BY(Locks::deoptimization_lock_);

  static size_t* GetReferenceCounterForEvent(uint32_t instrumentation_event);

  // Weak global type cache, TODO improve this.
  static TypeCache type_cache_ GUARDED_BY(Locks::alloc_tracker_lock_);

  // Instrumentation event reference counters.
  // TODO we could use an array instead of having all these dedicated counters. Instrumentation
  // events are bits of a mask so we could convert them to array index.
  static size_t dex_pc_change_event_ref_count_ GUARDED_BY(Locks::deoptimization_lock_);
  static size_t method_enter_event_ref_count_ GUARDED_BY(Locks::deoptimization_lock_);
  static size_t method_exit_event_ref_count_ GUARDED_BY(Locks::deoptimization_lock_);
  static size_t field_read_event_ref_count_ GUARDED_BY(Locks::deoptimization_lock_);
  static size_t field_write_event_ref_count_ GUARDED_BY(Locks::deoptimization_lock_);
  static size_t exception_catch_event_ref_count_ GUARDED_BY(Locks::deoptimization_lock_);
  static uint32_t instrumentation_events_ GUARDED_BY(Locks::mutator_lock_);

  friend class AllocRecord;  // For type_cache_ with proper annotalysis.
  DISALLOW_COPY_AND_ASSIGN(Dbg);
};

#define CHUNK_TYPE(_name) \
    static_cast<uint32_t>((_name)[0] << 24 | (_name)[1] << 16 | (_name)[2] << 8 | (_name)[3])

}  // namespace art

#endif  // ART_RUNTIME_DEBUGGER_H_
