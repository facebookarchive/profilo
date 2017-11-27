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

#ifndef ART_RUNTIME_JDWP_JDWP_H_
#define ART_RUNTIME_JDWP_JDWP_H_

#include "atomic.h"
#include "base/mutex.h"
#include "jdwp/jdwp_bits.h"
#include "jdwp/jdwp_constants.h"
#include "jdwp/jdwp_expand_buf.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vector>

struct iovec;

namespace art {

class ArtField;
class ArtMethod;
union JValue;
class Thread;

namespace mirror {
  class Class;
  class Object;
  class Throwable;
}  // namespace mirror
class Thread;

namespace JDWP {

/*
 * Fundamental types.
 *
 * ObjectId and RefTypeId must be the same size.
 * Its OK to change MethodId and FieldId sizes as long as the size is <= 8 bytes.
 * Note that ArtFields are 64 bit pointers on 64 bit targets. So this one must remain 8 bytes.
 */
typedef uint64_t FieldId;     /* static or instance field */
typedef uint64_t MethodId;    /* any kind of method, including constructors */
typedef uint64_t ObjectId;    /* any object (threadID, stringID, arrayID, etc) */
typedef uint64_t RefTypeId;   /* like ObjectID, but unique for Class objects */
typedef uint64_t FrameId;     /* short-lived stack frame ID */

ObjectId ReadObjectId(const uint8_t** pBuf);

static inline void SetFieldId(uint8_t* buf, FieldId val) { return Set8BE(buf, val); }
static inline void SetMethodId(uint8_t* buf, MethodId val) { return Set8BE(buf, val); }
static inline void SetObjectId(uint8_t* buf, ObjectId val) { return Set8BE(buf, val); }
static inline void SetRefTypeId(uint8_t* buf, RefTypeId val) { return Set8BE(buf, val); }
static inline void SetFrameId(uint8_t* buf, FrameId val) { return Set8BE(buf, val); }
static inline void expandBufAddFieldId(ExpandBuf* pReply, FieldId id) { expandBufAdd8BE(pReply, id); }
static inline void expandBufAddMethodId(ExpandBuf* pReply, MethodId id) { expandBufAdd8BE(pReply, id); }
static inline void expandBufAddObjectId(ExpandBuf* pReply, ObjectId id) { expandBufAdd8BE(pReply, id); }
static inline void expandBufAddRefTypeId(ExpandBuf* pReply, RefTypeId id) { expandBufAdd8BE(pReply, id); }
static inline void expandBufAddFrameId(ExpandBuf* pReply, FrameId id) { expandBufAdd8BE(pReply, id); }

struct EventLocation {
  ArtMethod* method;
  uint32_t dex_pc;
};

/*
 * Holds a JDWP "location".
 */
struct JdwpLocation {
  JdwpTypeTag type_tag;
  RefTypeId class_id;
  MethodId method_id;
  uint64_t dex_pc;
};
std::ostream& operator<<(std::ostream& os, const JdwpLocation& rhs)
    SHARED_REQUIRES(Locks::mutator_lock_);
bool operator==(const JdwpLocation& lhs, const JdwpLocation& rhs);
bool operator!=(const JdwpLocation& lhs, const JdwpLocation& rhs);

/*
 * How we talk to the debugger.
 */
enum JdwpTransportType {
  kJdwpTransportUnknown = 0,
  kJdwpTransportSocket,       // transport=dt_socket
  kJdwpTransportAndroidAdb,   // transport=dt_android_adb
};
std::ostream& operator<<(std::ostream& os, const JdwpTransportType& rhs);

struct JdwpOptions {
  JdwpTransportType transport = kJdwpTransportUnknown;
  bool server = false;
  bool suspend = false;
  std::string host = "";
  uint16_t port = static_cast<uint16_t>(-1);
};

bool operator==(const JdwpOptions& lhs, const JdwpOptions& rhs);

struct JdwpEvent;
class JdwpNetStateBase;
struct ModBasket;
class Request;

/*
 * State for JDWP functions.
 */
struct JdwpState {
  /*
   * Perform one-time initialization.
   *
   * Among other things, this binds to a port to listen for a connection from
   * the debugger.
   *
   * Returns a newly-allocated JdwpState struct on success, or nullptr on failure.
   *
   * NO_THREAD_SAFETY_ANALYSIS since we can't annotate that we do not have
   * state->thread_start_lock_ held.
   */
  static JdwpState* Create(const JdwpOptions* options)
      REQUIRES(!Locks::mutator_lock_) NO_THREAD_SAFETY_ANALYSIS;

  ~JdwpState();

  /*
   * Returns "true" if a debugger or DDM is connected.
   */
  bool IsActive();

  /**
   * Returns the Thread* for the JDWP daemon thread.
   */
  Thread* GetDebugThread();

  /*
   * Get time, in milliseconds, since the last debugger activity.
   */
  int64_t LastDebuggerActivity();

  void ExitAfterReplying(int exit_status);

  // Acquires/releases the JDWP synchronization token for the debugger
  // thread (command handler) so no event thread posts an event while
  // it processes a command. This must be called only from the debugger
  // thread.
  void AcquireJdwpTokenForCommand() REQUIRES(!jdwp_token_lock_);
  void ReleaseJdwpTokenForCommand() REQUIRES(!jdwp_token_lock_);

  // Acquires/releases the JDWP synchronization token for the event thread
  // so no other thread (debugger thread or event thread) interleaves with
  // it when posting an event. This must NOT be called from the debugger
  // thread, only event thread.
  void AcquireJdwpTokenForEvent(ObjectId threadId) REQUIRES(!jdwp_token_lock_);
  void ReleaseJdwpTokenForEvent() REQUIRES(!jdwp_token_lock_);

  /*
   * These notify the debug code that something interesting has happened.  This
   * could be a thread starting or ending, an exception, or an opportunity
   * for a breakpoint.  These calls do not mean that an event the debugger
   * is interested has happened, just that something has happened that the
   * debugger *might* be interested in.
   *
   * The item of interest may trigger multiple events, some or all of which
   * are grouped together in a single response.
   *
   * The event may cause the current thread or all threads (except the
   * JDWP support thread) to be suspended.
   */

  /*
   * The VM has finished initializing.  Only called when the debugger is
   * connected at the time initialization completes.
   */
  void PostVMStart() SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!jdwp_token_lock_);

  /*
   * A location of interest has been reached.  This is used for breakpoints,
   * single-stepping, and method entry/exit.  (JDWP requires that these four
   * events are grouped together in a single response.)
   *
   * In some cases "*pLoc" will just have a method and class name, e.g. when
   * issuing a MethodEntry on a native method.
   *
   * "eventFlags" indicates the types of events that have occurred.
   *
   * "returnValue" is non-null for MethodExit events only.
   */
  void PostLocationEvent(const EventLocation* pLoc, mirror::Object* thisPtr, int eventFlags,
                         const JValue* returnValue)
     REQUIRES(!event_list_lock_, !jdwp_token_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * A field of interest has been accessed or modified. This is used for field access and field
   * modification events.
   *
   * "fieldValue" is non-null for field modification events only.
   * "is_modification" is true for field modification, false for field access.
   */
  void PostFieldEvent(const EventLocation* pLoc, ArtField* field, mirror::Object* thisPtr,
                      const JValue* fieldValue, bool is_modification)
      REQUIRES(!event_list_lock_, !jdwp_token_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * An exception has been thrown.
   *
   * Pass in a zeroed-out "*pCatchLoc" if the exception wasn't caught.
   */
  void PostException(const EventLocation* pThrowLoc, mirror::Throwable* exception_object,
                     const EventLocation* pCatchLoc, mirror::Object* thisPtr)
      REQUIRES(!event_list_lock_, !jdwp_token_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * A thread has started or stopped.
   */
  void PostThreadChange(Thread* thread, bool start)
      REQUIRES(!event_list_lock_, !jdwp_token_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * Class has been prepared.
   */
  void PostClassPrepare(mirror::Class* klass)
      REQUIRES(!event_list_lock_, !jdwp_token_lock_) SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * The VM is about to stop.
   */
  bool PostVMDeath();

  // Called if/when we realize we're talking to DDMS.
  void NotifyDdmsActive() SHARED_REQUIRES(Locks::mutator_lock_);


  void SetupChunkHeader(uint32_t type, size_t data_len, size_t header_size, uint8_t* out_header);

  /*
   * Send up a chunk of DDM data.
   */
  void DdmSendChunkV(uint32_t type, const iovec* iov, int iov_count)
      SHARED_REQUIRES(Locks::mutator_lock_);

  bool HandlePacket() REQUIRES(!shutdown_lock_, !jdwp_token_lock_);

  void SendRequest(ExpandBuf* pReq);

  void ResetState()
      REQUIRES(!event_list_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  /* atomic ops to get next serial number */
  uint32_t NextRequestSerial();
  uint32_t NextEventSerial();

  void Run()
      REQUIRES(!Locks::mutator_lock_, !Locks::thread_suspend_count_lock_, !thread_start_lock_,
               !attach_lock_, !event_list_lock_);

  /*
   * Register an event by adding it to the event list.
   *
   * "*pEvent" must be storage allocated with jdwpEventAlloc().  The caller
   * may discard its pointer after calling this.
   */
  JdwpError RegisterEvent(JdwpEvent* pEvent)
      REQUIRES(!event_list_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * Unregister an event, given the requestId.
   */
  void UnregisterEventById(uint32_t requestId)
      REQUIRES(!event_list_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

  /*
   * Unregister all events.
   */
  void UnregisterAll()
      REQUIRES(!event_list_lock_)
      SHARED_REQUIRES(Locks::mutator_lock_);

 private:
  explicit JdwpState(const JdwpOptions* options);
  size_t ProcessRequest(Request* request, ExpandBuf* pReply, bool* skip_reply)
      REQUIRES(!jdwp_token_lock_);
  bool InvokeInProgress();
  bool IsConnected();
  void SuspendByPolicy(JdwpSuspendPolicy suspend_policy, JDWP::ObjectId thread_self_id)
      REQUIRES(!Locks::mutator_lock_);
  void SendRequestAndPossiblySuspend(ExpandBuf* pReq, JdwpSuspendPolicy suspend_policy,
                                     ObjectId threadId)
      SHARED_REQUIRES(Locks::mutator_lock_) REQUIRES(!jdwp_token_lock_);
  void CleanupMatchList(const std::vector<JdwpEvent*>& match_list)
      REQUIRES(event_list_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
  void EventFinish(ExpandBuf* pReq);
  bool FindMatchingEvents(JdwpEventKind eventKind, const ModBasket& basket,
                          std::vector<JdwpEvent*>* match_list)
      REQUIRES(!event_list_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
  void FindMatchingEventsLocked(JdwpEventKind eventKind, const ModBasket& basket,
                                std::vector<JdwpEvent*>* match_list)
      REQUIRES(event_list_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
  void UnregisterEvent(JdwpEvent* pEvent)
      REQUIRES(event_list_lock_) SHARED_REQUIRES(Locks::mutator_lock_);
  void SendBufferedRequest(uint32_t type, const std::vector<iovec>& iov);

  /*
   * When we hit a debugger event that requires suspension, it's important
   * that we wait for the thread to suspend itself before processing any
   * additional requests. Otherwise, if the debugger immediately sends a
   * "resume thread" command, the resume might arrive before the thread has
   * suspended itself.
   *
   * It's also important no event thread suspends while we process a command
   * from the debugger. Otherwise we could post an event ("thread death")
   * before sending the reply of the command being processed ("resume") and
   * cause bad synchronization with the debugger.
   *
   * The thread wanting "exclusive" access to the JDWP world must call the
   * SetWaitForJdwpToken method before processing a command from the
   * debugger or sending an event to the debugger.
   * Once the command is processed or the event thread has posted its event,
   * it must call the ClearWaitForJdwpToken method to allow another thread
   * to do JDWP stuff.
   *
   * Therefore the main JDWP handler loop will wait for the event thread
   * suspension before processing the next command. Once the event thread
   * has suspended itself and cleared the token, the JDWP handler continues
   * processing commands. This works in the suspend-all case because the
   * event thread doesn't suspend itself until everything else has suspended.
   *
   * It's possible that multiple threads could encounter thread-suspending
   * events at the same time, so we grab a mutex in the SetWaitForJdwpToken
   * call, and release it in the ClearWaitForJdwpToken call.
   */
  void SetWaitForJdwpToken(ObjectId threadId) REQUIRES(!jdwp_token_lock_);
  void ClearWaitForJdwpToken() REQUIRES(!jdwp_token_lock_);

 public:  // TODO: fix privacy
  const JdwpOptions* options_;

 private:
  /* wait for creation of the JDWP thread */
  Mutex thread_start_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable thread_start_cond_ GUARDED_BY(thread_start_lock_);

  pthread_t pthread_;
  Thread* thread_;

  volatile int32_t debug_thread_started_ GUARDED_BY(thread_start_lock_);
  ObjectId debug_thread_id_;

 private:
  bool run;

 public:  // TODO: fix privacy
  JdwpNetStateBase* netState;

 private:
  // For wait-for-debugger.
  Mutex attach_lock_ ACQUIRED_AFTER(thread_start_lock_);
  ConditionVariable attach_cond_ GUARDED_BY(attach_lock_);

  // Time of last debugger activity, in milliseconds.
  Atomic<int64_t> last_activity_time_ms_;

  // Global counters and a mutex to protect them.
  AtomicInteger request_serial_;
  AtomicInteger event_serial_;

  // Linked list of events requested by the debugger (breakpoints, class prep, etc).
  Mutex event_list_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER ACQUIRED_BEFORE(Locks::breakpoint_lock_);
  JdwpEvent* event_list_ GUARDED_BY(event_list_lock_);
  size_t event_list_size_ GUARDED_BY(event_list_lock_);  // Number of elements in event_list_.

  // Used to synchronize JDWP command handler thread and event threads so only one
  // thread does JDWP stuff at a time. This prevent from interleaving command handling
  // and event notification. Otherwise we could receive a "resume" command for an
  // event thread that is not suspended yet, or post a "thread death" or event "VM death"
  // event before sending the reply of the "resume" command that caused it.
  Mutex jdwp_token_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  ConditionVariable jdwp_token_cond_ GUARDED_BY(jdwp_token_lock_);
  ObjectId jdwp_token_owner_thread_id_;

  bool ddm_is_active_;

  // Used for VirtualMachine.Exit command handling.
  bool should_exit_;
  int exit_status_;

  // Used to synchronize runtime shutdown with JDWP command handler thread.
  // When the runtime shuts down, it needs to stop JDWP command handler thread by closing the
  // JDWP connection. However, if the JDWP thread is processing a command, it needs to wait
  // for the command to finish so we can send its reply before closing the connection.
  Mutex shutdown_lock_ ACQUIRED_AFTER(event_list_lock_);
  ConditionVariable shutdown_cond_ GUARDED_BY(shutdown_lock_);
  bool processing_request_ GUARDED_BY(shutdown_lock_);
};

std::string DescribeField(const FieldId& field_id) SHARED_REQUIRES(Locks::mutator_lock_);
std::string DescribeMethod(const MethodId& method_id) SHARED_REQUIRES(Locks::mutator_lock_);
std::string DescribeRefTypeId(const RefTypeId& ref_type_id) SHARED_REQUIRES(Locks::mutator_lock_);

class Request {
 public:
  Request(const uint8_t* bytes, uint32_t available);
  ~Request();

  std::string ReadUtf8String();

  // Helper function: read a variable-width value from the input buffer.
  uint64_t ReadValue(size_t width);

  int32_t ReadSigned32(const char* what);

  uint32_t ReadUnsigned32(const char* what);

  FieldId ReadFieldId() SHARED_REQUIRES(Locks::mutator_lock_);

  MethodId ReadMethodId() SHARED_REQUIRES(Locks::mutator_lock_);

  ObjectId ReadObjectId(const char* specific_kind);

  ObjectId ReadArrayId();

  ObjectId ReadObjectId();

  ObjectId ReadThreadId();

  ObjectId ReadThreadGroupId();

  RefTypeId ReadRefTypeId() SHARED_REQUIRES(Locks::mutator_lock_);

  FrameId ReadFrameId();

  template <typename T> T ReadEnum1(const char* specific_kind) {
    T value = static_cast<T>(Read1());
    VLOG(jdwp) << "    " << specific_kind << " " << value;
    return value;
  }

  JdwpTag ReadTag();

  JdwpTypeTag ReadTypeTag();

  JdwpLocation ReadLocation() SHARED_REQUIRES(Locks::mutator_lock_);

  JdwpModKind ReadModKind();

  //
  // Return values from this JDWP packet's header.
  //
  size_t GetLength() { return byte_count_; }
  uint32_t GetId() { return id_; }
  uint8_t GetCommandSet() { return command_set_; }
  uint8_t GetCommand() { return command_; }

  // Returns the number of bytes remaining.
  size_t size() { return end_ - p_; }

  // Returns a pointer to the next byte.
  const uint8_t* data() { return p_; }

  void Skip(size_t count) { p_ += count; }

  void CheckConsumed();

 private:
  uint8_t Read1();
  uint16_t Read2BE();
  uint32_t Read4BE();
  uint64_t Read8BE();

  uint32_t byte_count_;
  uint32_t id_;
  uint8_t command_set_;
  uint8_t command_;

  const uint8_t* p_;
  const uint8_t* end_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

}  // namespace JDWP

}  // namespace art

#endif  // ART_RUNTIME_JDWP_JDWP_H_
