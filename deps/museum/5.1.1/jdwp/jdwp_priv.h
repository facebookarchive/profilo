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
#define kJDWPHeaderLen  11
#define kJDWPFlagReply  0x80

#define kMagicHandshake     "JDWP-Handshake"
#define kMagicHandshakeLen  (sizeof(kMagicHandshake)-1)

/* DDM support */
#define kJDWPDdmCmdSet  199     /* 0xc7, or 'G'+128 */
#define kJDWPDdmCmd     1

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

  ssize_t WritePacket(ExpandBuf* pReply, size_t length) LOCKS_EXCLUDED(socket_lock_);
  ssize_t WriteBufferedPacket(const std::vector<iovec>& iov) LOCKS_EXCLUDED(socket_lock_);

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
