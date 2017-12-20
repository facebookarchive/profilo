// Copyright 2004-present Facebook. All Rights Reserved.


#pragma once

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

 private:
  std::function<void()> jailed_;
  uint32_t timeout_sec_;

  static void alarm_handler(int signum);
};

} // forkjail
} // facebook
