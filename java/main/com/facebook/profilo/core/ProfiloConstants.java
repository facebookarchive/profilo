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

package com.facebook.profilo.core;

public final class ProfiloConstants {

  public static final int NONE = 0;
  public static final int NO_CALLSITE = 0;
  public static final int NO_MATCH = 0;
  public static final int TRACING_DISABLED = -1;

  /* Keep in sync with AbortReason.h */
  public static final int ABORT_REASON_UNKNOWN = 1;
  public static final int ABORT_REASON_CONTROLLER_INITIATED = 2;
  public static final int ABORT_REASON_MISSED_EVENT = 3;
  public static final int ABORT_REASON_TIMEOUT = 4;
  public static final int ABORT_REASON_NEW_START = 5;

  // Things in the remote process can go wrong for the same reason as in the
  // main process. Thus, just mark if the reason is "remote" by using a single
  // bit
  public static final int REMOTE_PROCESS_BIT = 0x80000000;

  public static int packRemoteAbortReason(int abortReason) {
    return (abortReason | REMOTE_PROCESS_BIT);
  }

  public static int unpackRemoteAbortReason(int remoteAbortReason) {
    return (remoteAbortReason & ~REMOTE_PROCESS_BIT);
  }

  public static boolean isRemoteAbort(int abortReason) {
    return (abortReason & REMOTE_PROCESS_BIT) == REMOTE_PROCESS_BIT;
  }

  public static String abortReasonName(int abortReason) {
    switch (abortReason) {
      case ABORT_REASON_UNKNOWN:
        return "unknown";
      case ABORT_REASON_CONTROLLER_INITIATED:
        return "controller_init";
      case ABORT_REASON_MISSED_EVENT:
        return "missed_event";
      case ABORT_REASON_TIMEOUT:
        return "timeout";
      case ABORT_REASON_NEW_START:
        return "new_start";
    }
    return "UNKNOWN REASON " + abortReason;
  }
}
