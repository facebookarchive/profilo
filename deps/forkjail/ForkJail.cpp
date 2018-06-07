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


#include "ForkJail.h"
#include "linux_syscall_support.h"

#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <system_error>
#include <pthread.h>
#include <string>

namespace facebook {
namespace forkjail {

namespace {
class SignalMask {

public:
  SignalMask(sigset_t& newmask) {
    if (pthread_sigmask(SIG_SETMASK, &newmask, &old_)) {
      throw std::system_error(errno, std::system_category(), "sigprocmask");
    }
  }

  ~SignalMask() {
    if (pthread_sigmask(SIG_SETMASK, &old_, NULL)) {
      throw std::system_error(errno, std::system_category(), "sigprocmask restore");
    }
  }

private:
  sigset_t old_;
};

inline static std::system_error errno_error(std::string error) {
  return std::system_error(errno, std::system_category(), error);
}


inline pid_t real_fork() {
#ifndef ANDROID
  // Assume we don't have to go through these hoops for non-Android code.
  return fork();
#else
  return syscall(
      __NR_clone,
      /*flags*/ CLONE_CHILD_CLEARTID | SIGCHLD,
      /*child_stack*/ NULL,
      /*ptid*/ NULL,
      /*ctid*/ NULL,
      /*regs*/ NULL);
#endif
}

inline int real_sigaction(
    int signum,
    const struct kernel_sigaction *act,
    struct kernel_sigaction *oldact) {

#ifndef ANDROID
  // Assume we don't have to go through these hoops for non-Android code.
  return sys_sigaction(signum, act, oldact);
#else
  return sys_rt_sigaction(signum, act, oldact, sizeof(kernel_sigset_t));
#endif
}

} // namespace

ForkJail::ForkJail(
  std::function<void()> jailed,
  uint32_t timeout_sec
): jailed_(jailed), timeout_sec_(timeout_sec) {}

pid_t ForkJail::forkAndRun() {
  sigset_t everything, nothing;

  if (sigfillset(&everything) == -1){
    throw errno_error("sigfillset");
  }

  if (sigemptyset(&nothing) == -1){
    throw errno_error("sigemptyset");
  }

  {
    SignalMask mask(everything);

    //
    // Facebook-specific workaround.
    //
    // In specific configurations on art, fb4a distracts fork() by replacing the
    // first instruction. In this mode, distract relies on signals
    // (SIGSEGV/SIGILL/SIGBUS) to execute the hook.
    //
    // However, we have currently blocked all signals in order to safely
    // set up the child without unexpected interruptions.
    // Therefore, execute a bare clone(2) call without relying on the
    // distracted libc wrapper.
    //
    // Caveat: the pthread state inside the child will be corrupted - the
    // thread won't know its own tid and the cached pid will be wrong. Use
    // gettid() and getpid() instead.
    //
    pid_t ret = real_fork();
    if (ret == -1) {
      // parent
      throw errno_error("fork");
    }

    if (ret != 0) {
      // parent
      return ret;
    }

    // child

    // Prevent java.lang.ProcessManager.watchChildren from waiting for this
    // process
    if (setpgid(0, 0)) {
      //LOGE("setpgid failed: %s", strerror(errno));
      _exit(kChildSetupExitCode);
    }

    // Restore the signal handlers to their default values.
    struct kernel_sigaction dfltaction;
    dfltaction.sa_handler_ = SIG_DFL;
    dfltaction.sa_flags = 0;

    if(sys_sigemptyset(&dfltaction.sa_mask)) {
      throw errno_error("sigemptyset(dfltaction)");
    }

    for (int signum = 1; signum < NSIG; ++signum) {
      // Skip signals we can't intercept, as well as NPTL-reserved signals.
      if (signum == SIGKILL || signum == SIGSTOP ||
          (signum >= 32 && signum < SIGRTMIN)) {
        continue;
      }

      // Similarly to fork() above, siagaction may have been distracted.
      if (real_sigaction(signum, &dfltaction, NULL)) {
        //LOGE("sigaction failed: %s", strerror(errno));
        _exit(kChildSetupExitCode);
      }
    }

    // Set an alarm handler which exits with a different exit code.

    struct kernel_sigaction alarm_act;
    alarm_act.sa_handler_ = &ForkJail::alarm_handler;
    alarm_act.sa_flags = 0;

    if (sys_sigfillset(&alarm_act.sa_mask)) {
      //LOGE("sigemptyset in child: %s", strerror(errno));
      _exit(kChildSetupExitCode);
    }

    if (real_sigaction(SIGALRM, &alarm_act, NULL)) {
      //LOGE("sigaction(SIGALRM) failed: %s", strerror(errno));
      _exit(kChildSetupExitCode);
    }
  } // end SignalMask

  // At this point, the child has restored to the parent's signal mask, restore
  // it again to the empty signal mask, so we lose all signal handling state
  // from the parent (for example, the parent may have had SIGALRM blocked).

  if (sigprocmask(SIG_SETMASK, &nothing, NULL)) {
    //LOGE("pthread_sigmask unblock all: %s", strerror(errno));
    _exit(kChildSetupExitCode);
  }

  // Set our timeout alarm and run the jailed code.
  alarm(timeout_sec_);

  jailed_();

  return 0;
}

void ForkJail::alarm_handler(int signum){
  _exit(kChildTimeoutExitCode);
}

} // forkjail
} // facebook
