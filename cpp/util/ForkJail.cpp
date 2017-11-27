// Copyright 2004-present Facebook. All Rights Reserved.


#include "ForkJail.h"

#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <system_error>
#include <pthread.h>
#include <string>

namespace facebook {
namespace loom {
namespace util {

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

inline static std::system_error errno_error(std::string error){
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
    const struct sigaction *act,
    struct sigaction *oldact) {

#ifndef ANDROID
  // Assume we don't have to go through these hoops for non-Android code.
  return sigaction(signum, act, oldact);
#else
  return syscall(__NR_sigaction, signum, act, oldact);
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
    struct sigaction dfltaction;
    dfltaction.sa_handler = SIG_DFL;
    dfltaction.sa_flags = 0;

    if(sigemptyset(&dfltaction.sa_mask)) {
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

    struct sigaction alarm_act;
    alarm_act.sa_handler = &ForkJail::alarm_handler;
    alarm_act.sa_flags = 0;

    if (sigfillset(&alarm_act.sa_mask)) {
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

} // util
} // loom
} // facebook
