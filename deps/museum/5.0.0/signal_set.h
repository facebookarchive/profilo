/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_SIGNAL_SET_H_
#define ART_RUNTIME_SIGNAL_SET_H_

#include <signal.h>

#include "base/logging.h"

namespace art {

class SignalSet {
 public:
  SignalSet() {
    if (sigemptyset(&set_) == -1) {
      PLOG(FATAL) << "sigemptyset failed";
    }
  }

  void Add(int signal) {
    if (sigaddset(&set_, signal) == -1) {
      PLOG(FATAL) << "sigaddset " << signal << " failed";
    }
  }

  void Block() {
    if (sigprocmask(SIG_BLOCK, &set_, NULL) == -1) {
      PLOG(FATAL) << "sigprocmask failed";
    }
  }

  int Wait() {
    // Sleep in sigwait() until a signal arrives. gdb causes EINTR failures.
    int signal_number;
    int rc = TEMP_FAILURE_RETRY(sigwait(&set_, &signal_number));
    if (rc != 0) {
      PLOG(FATAL) << "sigwait failed";
    }
    return signal_number;
  }

 private:
  sigset_t set_;
};

}  // namespace art

#endif  // ART_RUNTIME_SIGNAL_SET_H_
