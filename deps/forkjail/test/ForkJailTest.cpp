// Copyright 2004-present Facebook. All Rights Reserved.

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <forkjail/ForkJail.h>
#include <gtest/gtest.h>

#include <system_error>

#include <iostream>

namespace facebook {
namespace forkjail {

namespace {

int waitForChild(pid_t child) {
  int status = 0;
  do {
    if (waitpid(child, &status, 0) != child) {
      throw std::system_error(errno, std::system_category(), "waitpid");
    }
  } while(!WIFEXITED(status) && !WIFSIGNALED(status));
  if (!WIFEXITED(status)) {
    throw std::logic_error("Child terminated uncleanly");
  }
  return WEXITSTATUS(status);
}

void EXPECT_CHILD_STATUS(ForkJail& jail, int status) {
  auto child = jail.forkAndRun();
  EXPECT_NE(0, child); // the child never reaches here

  auto exit = waitForChild(child);
  EXPECT_EQ(status, exit);
}

} // namespace

TEST(ForkJail, testNoopRun) {
  static constexpr int kChildStatus = 123;
  static constexpr int kTimeoutSec = 10;

  ForkJail jail([] {
    exit(kChildStatus); // don't let the child return from forkAndRun
  }, kTimeoutSec);

  EXPECT_CHILD_STATUS(jail, kChildStatus);
}

TEST(ForkJail, testTimeout) {
  static constexpr int kChildSleepSec = 120;
  static constexpr int kTimeoutSec = 1;

  // EXPECT_EQ takes the expected value by reference, which requires us
  // to allocate static storage for kChildTimeoutExitCode. We don't want
  // to do that so allocate static storage for a copy instead.
  static constexpr int kTimeoutExitCode = ForkJail::kChildTimeoutExitCode;

  ForkJail jail([] {
    sleep(kChildSleepSec);
  }, kTimeoutSec);

  EXPECT_CHILD_STATUS(jail, kTimeoutExitCode);
}

TEST(ForkJail, testSignalMaskEmpty) {
  static constexpr int kTimeoutSec = 10;
  static constexpr int kExitCodeSuccess = 10;
  static constexpr int kExitCodeFailure = 11;

  ForkJail jail([] {
    sigset_t mask;
    if (sigprocmask(SIG_SETMASK, NULL, &mask)) {
      exit(kExitCodeFailure);
    }

    // Expect the mask to be empty except for NPTL signals
    for (int sig = 1; sig < NSIG; sig++) {
      if (sig >= 32 && sig < SIGRTMIN) {
        // reserved signals, skip
        continue;
      }
      if (sigismember(&mask, sig)) {
        exit(kExitCodeFailure);
      }
    }
    exit(kExitCodeSuccess);
  }, kTimeoutSec);

  EXPECT_CHILD_STATUS(jail, kExitCodeSuccess);
}

TEST(ForkJail, testProcessGroupChanged) {
  static constexpr int kTimeoutSec = 10;
  static constexpr int kExitCodeSuccess = 10;
  static constexpr int kExitCodeFailure = 11;

  pid_t ppgid = getpgid(0);
  pid_t ppid = getpid();

  ForkJail jail([ppid, ppgid] {
    pid_t pgid = getpgid(0);

    if (pgid == getpid()) {
      exit(kExitCodeSuccess);
    } else {
      exit(kExitCodeFailure);
    }
  }, kTimeoutSec);

  EXPECT_CHILD_STATUS(jail, kExitCodeSuccess);
}

} // forkjail
} // facebook
