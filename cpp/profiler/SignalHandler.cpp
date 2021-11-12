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

#include "SignalHandler.h"

#include <dlfcn.h>
#include <stdexcept>
#include <string>
#include <system_error>

#include <abort_with_reason.h>

namespace facebook {
namespace profilo {
namespace profiler {

std::atomic<SignalHandler*>
    SignalHandler::globalRegisteredSignalHandlers[NSIG]{};
std::once_flag SignalHandler::globalPhaserInit{};
phaser_t SignalHandler::globalPhaser{};

void SignalHandler::UniversalHandler(
    int signum,
    siginfo_t* siginfo,
    void* ucontext) {
  // Sequencing:
  // We enter the global phaser first, then we enter the local phaser,
  // then we exit the global phaser, and then we exit the local phaser.

  auto phase = phaser_enter(&globalPhaser);
  HandlerScope scope = SignalHandler::EnterHandler(signum);
  phaser_exit(&globalPhaser, phase);

  if (!scope.IsEnabled()) {
    scope.CallPreviousHandler(signum, siginfo, ucontext);
    return;
  }

  scope.handler_.handler_(std::move(scope), signum, siginfo, ucontext);
}

SignalHandler& SignalHandler::Initialize(int signum, HandlerPtr handler) {
  if (handler == nullptr) {
    throw std::invalid_argument(
        "null HandlerPtr is not allowed, use Disable() instead");
  }
  // Initialize on first use
  std::call_once(globalPhaserInit, [] {
    if (phaser_init(&globalPhaser)) {
      throw std::logic_error("Could not initialize global phaser");
    }
  });

  while (true) {
    auto* lookup = globalRegisteredSignalHandlers[signum].load();
    auto* instance = new SignalHandler(signum, handler);

    if (lookup != nullptr) {
      // Propagate the "original" sigaction to our new handler.
      instance->old_sigaction_ = lookup->old_sigaction_;
    }

    if (globalRegisteredSignalHandlers[signum].compare_exchange_strong(
            lookup, instance)) {
      if (lookup == nullptr) {
        // No previous SignalHandler instance, must take over the signal.
        SignalHandler::AndroidAwareSigaction(
            signum, SignalHandler::UniversalHandler, &instance->old_sigaction_);
      } else {
        // Have a previous SignalHandler instance, we must wait for users to
        // finish first.
        //
        // Sequencing:
        //
        // Reads:
        // Global phaser [     ]
        // Atomic load     []
        // Local phaser       [    ]
        //              ^    ^ ^
        //              1    2 3
        //
        // After we've replaced the value in the global list, concurrent users
        // fall in one of 3 cases:
        // 1. They've read the new value (nothing to do)
        // 2. They've read the old value but have not entered the old instance's
        // phaser yet
        // 3. They've read the old value and have entered the old instance's
        // phaser
        //

        phaser_drain(&globalPhaser);
        // After this point, no concurrent users that saw the old value can be
        // in the global phaser without also being in the old instance's local
        // phaser.

        phaser_drain(&lookup->phaser_);
        // After this point, no concurrent user can have a reference to the old
        // instance, so it is safe to delete it.

        delete lookup;
      }

      return *instance;
    } else {
      // Someone beat us to it, try again by re-reading lookup and
      // re-verifying its state.
      delete instance;
      continue;
    }
  }
}

SignalHandler::SignalHandler(int signum, HandlerPtr handler)
    : signum_(signum),
      handler_(handler),
      data_(nullptr),
      phaser_(),
      enabled_(false),
      old_sigaction_() {
  if (phaser_init(&phaser_)) {
    throw std::runtime_error("Could not initialize phaser");
  }
}

SignalHandler::~SignalHandler() {
  phaser_destroy(&phaser_);
}

void SignalHandler::Enable() {
  enabled_.store(true, std::memory_order_relaxed);
}

void SignalHandler::Disable() {
  enabled_.store(false, std::memory_order_relaxed);
  phaser_drain(&phaser_);
}

SignalHandler::HandlerScope SignalHandler::EnterHandler(int signum) {
  auto* handler = globalRegisteredSignalHandlers[signum].load();
  if (!handler) {
    abortWithReason("EnterHandler call but no registered SignalHandler");
  }

  return SignalHandler::HandlerScope(*handler);
}

void SignalHandler::CallPreviousHandler(
    int signum,
    siginfo_t* info,
    void* ucontext) {
  sigset_t oldsigs;
  // Actually calls sigchain's sigprocmask wrapper but that's okay.
  // The sigchain wrapper only really cares about not masking signals
  // that sigchain has special handlers for. That type of mask modification
  // is perfectly okay with us.
  if (sigprocmask(SIG_SETMASK, &old_sigaction_.sa_mask, &oldsigs)) {
    abortWithReason("Cannot change signal mask");
  }
  if (old_sigaction_.sa_flags & SA_SIGINFO) {
    if (old_sigaction_.sa_sigaction != nullptr) {
      old_sigaction_.sa_sigaction(signum, info, ucontext);
    }
  } else if (old_sigaction_.sa_handler != nullptr) {
    if (old_sigaction_.sa_handler != SIG_DFL &&
        old_sigaction_.sa_handler != SIG_IGN) {
      old_sigaction_.sa_handler(signum);
    }
  }
  if (sigprocmask(SIG_SETMASK, &oldsigs, nullptr)) {
    abortWithReason("Cannot restore signal mask");
  }
}

void SignalHandler::AndroidAwareSigaction(
    int signum,
    SigactionPtr handler,
    struct sigaction* oldact) {
#ifdef __ANDROID__
  //
  // On Android, we need to look up sigaction64 or sigaction directly from libc
  // or otherwise we'll get the wrappers from sigchain.
  //
  // These wrappers do not work for our purposes because they run art's signal
  // handling before our handler and that signal handling can be misled to
  // believe it's in art code when in fact it's in SamplingProfiler unwinding.
  //
  // Further, we must use sigaction64 if available as otherwise we may hit a bug
  // in bionic where sigaction`libc calls sigaction64`sigchain.
  // See commit 11623dd60dd0f531fbc1cbf108680ba850acaf2f in AOSP.
  //
  static struct {
    int (*libc_sigaction64)(
        int,
        const struct sigaction64*,
        struct sigaction64*) = nullptr;
    int (*libc_sigemptyset64)(sigset64_t*) = nullptr;
    int (*libc_sigismember64)(sigset64_t*, int) = nullptr;
    int (*libc_sigaction)(int, const struct sigaction*, struct sigaction*) =
        nullptr;
    bool lookups_complete = false;
  } signal_state;

  if (!signal_state.lookups_complete) {
    auto libc = dlopen("libc.so", RTLD_LOCAL);
    if (!libc) {
      std::string error("Missing libc.so: ");
      throw std::runtime_error(error + dlerror());
    }

    signal_state.libc_sigaction64 =
        reinterpret_cast<decltype(signal_state.libc_sigaction64)>(
            dlsym(libc, "sigaction64"));

    if (signal_state.libc_sigaction64) {
      signal_state.libc_sigemptyset64 =
          reinterpret_cast<decltype(signal_state.libc_sigemptyset64)>(
              dlsym(libc, "sigemptyset64"));

      signal_state.libc_sigismember64 =
          reinterpret_cast<decltype(signal_state.libc_sigismember64)>(
              dlsym(libc, "sigismember64"));
    } else {
      signal_state.libc_sigaction =
          reinterpret_cast<decltype(signal_state.libc_sigaction)>(
              dlsym(libc, "sigaction"));
    }
    signal_state.lookups_complete = true;

    dlclose(libc);
  }

  int result = 0;
  if (signal_state.libc_sigaction64) {
    //
    // sigaction64 is available.
    // Convert from struct sigaction to struct sigaction64 and back
    // and call it directly.
    //
    // Note that the conversion from sigset64_t to sigset_t is lossy,
    // we lose real-time signals!
    //
    struct sigaction64 action64 {
      .sa_sigaction = handler, .sa_flags = kSignalHandlerFlags,
    };

    signal_state.libc_sigemptyset64(&action64.sa_mask);
    struct sigaction64 oldaction64;
    result = signal_state.libc_sigaction64(signum, &action64, &oldaction64);
    struct sigaction oldaction {
      .sa_flags = oldaction64.sa_flags,
    };

    if (oldaction.sa_flags & SA_SIGINFO) {
      oldaction.sa_sigaction = oldaction64.sa_sigaction;
    } else {
      oldaction.sa_handler = oldaction64.sa_handler;
    }

    sigemptyset(&oldaction.sa_mask);
    for (int i = 0; i < NSIG; i++) {
      if (signal_state.libc_sigismember64(&oldaction64.sa_mask, i)) {
        sigaddset(&oldaction.sa_mask, i);
      }
    }
    *oldact = oldaction;
  } else {
    struct sigaction action {
      .sa_sigaction = handler, .sa_flags = kSignalHandlerFlags,
    };
    sigemptyset(&action.sa_mask);
    result = signal_state.libc_sigaction(signum, &action, oldact);
  }

  if (result != 0) {
    throw std::system_error(errno, std::system_category());
  }
#else // not __ANDROID__
  struct sigaction action {};
  action.sa_flags = kSignalHandlerFlags;
  action.sa_sigaction = handler;
  sigemptyset(&action.sa_mask);
  if (sigaction(signum, &action, oldact)) {
    throw std::system_error(errno, std::system_category());
  }
#endif
}

} // namespace profiler
} // namespace profilo
} // namespace facebook
