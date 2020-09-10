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

#include <phaser.h>
#include <setjmp.h>
#include <signal.h>
#include <atomic>

namespace facebook {
namespace profilo {
namespace profiler {

//
// SignalHandler provides signal handling niceties for use by SamplingProfiler.
// In particular, it has the following main facilities:
//
// 1. Ability to look up associated pointers from within a signal handler
// context (Set/GetData).
// 2. Stronger guarantees when disabling the signal handler (all concurrent
// are guaranteed to have exited when Disable() returns).
// 3. RAII helpers for use inside signal handlers (HandlerScope).
// 4. Correct installation of the signal handler before all other signal
// handlers when running on Android.
//
// Signal handlers should use the EnterHandler static method to announce
// themselves and gain partial access to the relevant SignalHandler instance
// (via the methods on the HandlerScope).
//
// Once a SignalHandler has been installed for a particular signal, the handler
// function (i.e. HandlerPtr value passed to Initialize) *cannot* be changed.
// This is the primary reason this implementation is only useful for
// SamplingProfiler's purposes.
//
class SignalHandler {
 public:
  using HandlerPtr = void (*)(int, siginfo_t*, void*);

  //
  // Enable processing of signals by this SignalHandler.
  // Installs the signal handler, if not yet installed.
  // Calls to HandlerScope::IsEnabled will return true after this.
  //
  void Enable();

  //
  // Disables processing of signals by this SignalHandler.
  // Blocks until all signal handlers running at the time of this call
  // exit their respective HandlerScopes.
  //
  void Disable();

  void SetData(void* data) {
    data_.store(data, std::memory_order_relaxed);
  }

  class HandlerScope {
   private:
    explicit HandlerScope(SignalHandler& handler) : handler_(handler) {
      enabled_ = false;
      if (!handler_.initialized_) {
        return;
      }
      if (!handler_.enabled_.load(std::memory_order_relaxed)) {
        return;
      }
      phase_ = phaser_enter(&handler_.phaser_);
      enabled_ = true;
    }

    SignalHandler& handler_;
    bool enabled_;
    phaser_phase phase_;

    friend class SignalHandler;

   public:
    HandlerScope(HandlerScope&& other)
        : handler_(other.handler_),
          enabled_(other.enabled_),
          phase_(other.phase_) {
      other.enabled_ = false;
    }
    HandlerScope& operator=(HandlerScope&& other) {
      enabled_ = other.enabled_;
      phase_ = other.phase_;
      other.enabled_ = false;
      return *this;
    }

    HandlerScope(const HandlerScope&) = delete;
    HandlerScope& operator=(const HandlerScope&) = delete;

    virtual ~HandlerScope() {
      if (enabled_) {
        phaser_exit(&handler_.phaser_, phase_);
      }
    }

    bool IsEnabled() const {
      return enabled_;
    }

    void* GetData() const {
      return handler_.data_.load(std::memory_order_relaxed);
    }

    //
    // Exits this HandlerScope, then performs a siglongjmp call.
    //
    void siglongjmp(sigjmp_buf& env, int val) {
      if (enabled_) {
        phaser_exit(&handler_.phaser_, phase_);
        enabled_ = false;
      }
      ::siglongjmp(env, val);
    }

    //
    // Exits this HandlerScope, then calls the previous signal handler.
    //
    void CallPreviousHandler(int signum, siginfo_t* info, void* ucontext) {
      if (enabled_) {
        phaser_exit(&handler_.phaser_, phase_);
        enabled_ = false;
      }
      handler_.CallPreviousHandler(signum, info, ucontext);
    }
  };

  static SignalHandler& Initialize(int signum, HandlerPtr handler);

  //
  // Call first thing from a registered signal handler function.
  //
  static SignalHandler::HandlerScope EnterHandler(int signum);

 private:
  friend class SignalHandler::HandlerScope;
  friend class SignalHandlerTestAccessor;

  SignalHandler(int signum, HandlerPtr handler);
  SignalHandler(const SignalHandler&) = delete;
  SignalHandler& operator=(const SignalHandler&) = delete;
  virtual ~SignalHandler();

  static constexpr auto kSignalHandlerFlags =
      SA_SIGINFO | SA_NODEFER | SA_ONSTACK | SA_RESTART;

  // global storage to facilitate looking up a SignalHandler instance from
  // within a signal handler context
  static std::atomic<SignalHandler*> globalRegisteredSignalHandlers[NSIG];

  int signum_;
  HandlerPtr handler_;
  std::atomic<void*> data_;

  phaser_t phaser_;

  bool initialized_;
  std::atomic<bool> enabled_;

  struct sigaction old_sigaction_;

  static void AndroidAwareSigaction(
      int signum,
      HandlerPtr handler,
      struct sigaction* oldact);

  void CallPreviousHandler(int signum, siginfo_t*, void*);
};

} // namespace profiler
} // namespace profilo
} // namespace facebook
