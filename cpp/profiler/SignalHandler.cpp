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

SignalHandler& SignalHandler::Initialize(int signum, HandlerPtr handler) {
  while (true) {
    auto* lookup = globalRegisteredSignalHandlers[signum].load();
    if (lookup) {
      if (lookup->handler_ != handler) {
        throw std::logic_error(
            "SignalHandler::Initialize called with more than one handler!");
      }
      return *lookup;
    }

    // Lookup is null, let's try and instantiate the one and only SignalHandler
    auto* instance = new SignalHandler(signum, handler);
    if (globalRegisteredSignalHandlers[signum].compare_exchange_strong(
            lookup, instance)) {
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
      initialized_(false),
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
  if (!initialized_) {
    // Must take over the signal first.
    AndroidAwareSigaction(signum_, handler_, &old_sigaction_);
    initialized_ = true;
  }
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
  if (!initialized_) {
    return;
  }

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
    HandlerPtr handler,
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
