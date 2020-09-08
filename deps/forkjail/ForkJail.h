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


#pragma once

#include <cinttypes>
#include <sys/types.h>
#include <functional>

namespace facebook {
namespace forkjail {

// A utility class that forks and resets signal handlers in order to prevent a
// dangerous operation from taking down our own process.
class ForkJail {

public:
  // The execution of the child took longer than the specified timeout.
  static constexpr int kChildTimeoutExitCode = 253;

  // The jail could not be set up but the new process was still created.
  // This should be treated equivalently to an exception from forkAndRun.
  static constexpr int kChildSetupExitCode = 254;

  // jailed - the function to execute in a new process. Must not change the
  //          signal mask of the process. Can define new signal handlers if it
  //          delegates to the existing ones as well.
  //          Must not use any pthread APIs, use the libc/kernel interface
  //          instead.
  //
  // timeout_sec - the maximum amount of time the process will be alive before
  //               it's terminated.
  ForkJail(std::function<void()> jailed, uint32_t timeout_sec);

  // Executes the jailed function. Returns the value from fork(2).
  // Throws an exception if the fork does not succeed.
  //
  // The forked process has an empty signal mask and no non-default signal
  // handlers installed.
  pid_t forkAndRun();

  // Executes the real exit. This bypasses any hooks on _exit
  static void real_exit(int status);

 private:
  std::function<void()> jailed_;
  uint32_t timeout_sec_;

  static void alarm_handler(int signum);
};

} // forkjail
} // facebook
