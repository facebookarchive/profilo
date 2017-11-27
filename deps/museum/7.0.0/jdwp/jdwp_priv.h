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
 * JDWP internal interfaces.
 */
#ifndef ART_RUNTIME_JDWP_JDWP_PRIV_H_
#define ART_RUNTIME_JDWP_JDWP_PRIV_H_

#include "debugger.h"
#include "jdwp/jdwp.h"
#include "jdwp/jdwp_event.h"

#include <pthread.h>
#include <sys/uio.h>

/*
 * JDWP constants.
 */
static constexpr size_t kJDWPHeaderSizeOffset = 0U;
static constexpr size_t kJDWPHeaderIdOffset = 4U;
static constexpr size_t kJDWPHeaderFlagsOffset = 8U;
static constexpr size_t kJDWPHeaderErrorCodeOffset = 9U;
static constexpr size_t kJDWPHeaderCmdSetOffset = 9U;
static constexpr size_t kJDWPHeaderCmdOffset = 10U;
static constexpr size_t kJDWPHeaderLen = 11U;
static constexpr uint8_t kJDWPFlagReply = 0x80;

static constexpr const char kMagicHandshake[] = "JDWP-Handshake";
static constexpr size_t kMagicHandshakeLen = sizeof(kMagicHandshake) - 1;

/* Invoke commands */
static constexpr uint8_t kJDWPClassTypeCmdSet = 3U;
static constexpr uint8_t kJDWPClassTypeInvokeMethodCmd = 3U;
static constexpr uint8_t kJDWPClassTypeNewInstanceCmd = 4U;
static constexpr uint8_t kJDWPInterfaceTypeCmdSet = 5U;
static constexpr uint8_t kJDWPInterfaceTypeInvokeMethodCmd = 1U;
static constexpr uint8_t kJDWPObjectReferenceCmdSet = 9U;
static constexpr uint8_t kJDWPObjectReferenceInvokeCmd = 6U;

/* Event command */
static constexpr uint8_t kJDWPEventCmdSet = 64U;
static constexpr uint8_t kJDWPEventCompositeCmd = 100U;

/* DDM support */
static constexpr uint8_t kJDWPDdmCmdSet = 199U;  // 0xc7, or 'G'+128
static constexpr uint8_t kJDWPDdmCmd = 1U;

namespace art {

namespace JDWP {

struct JdwpState;

bool InitSocketTransport(JdwpState*, const JdwpOptions*);
bool InitAdbTransport(JdwpState*, const JdwpOptions*);

/*
 * Base class for the adb and socket JdwpNetState implementations.
 */
class JdwpNetStateBase {
 public:
  explicit JdwpNetStateBase(JdwpState*);
  virtual ~JdwpNetStateBase();

  virtual bool Accept() = 0;
  virtual bool Establish(const JdwpOptions*) = 0;
  virtual void Shutdown() = 0;
  virtual bool ProcessIncoming() = 0;

  void ConsumeBytes(size_t byte_count);

  bool IsConnected();

  bool IsAwaitingHandshake();

  void Close();

  ssize_t WritePacket(ExpandBuf* pReply, size_t length) REQUIRES(!socket_lock_);
  ssize_t WriteBufferedPacket(const std::vector<iovec>& iov) REQUIRES(!socket_lock_);
  Mutex* GetSocketLock() {
    return &socket_lock_;
  }
  ssize_t WriteBufferedPacketLocked(const std::vector<iovec>& iov);

  int clientSock;  // Active connection to debugger.

  int wake_pipe_[2];  // Used to break out of select.

  uint8_t input_buffer_[8192];
  size_t input_count_;

 protected:
  bool HaveFullPacket();

  bool MakePipe();
  void WakePipe();

  void SetAwaitingHandshake(bool new_state);

  JdwpState* state_;

 private:
  // Used to serialize writes to the socket.
  Mutex socket_lock_;

  // Are we waiting for the JDWP handshake?
  bool awaiting_handshake_;
};

}  // namespace JDWP

}  // namespace art

#endif  // ART_RUNTIME_JDWP_JDWP_PRIV_H_
