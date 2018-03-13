/**
 * Copyright 2018-present, Facebook, Inc.
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

/*
 * The sigmux library provides a central point for cooperative
 * signal handling in a single process on POSIX systems.
 *
 * The POSIX signal API associates signals with handlers for an entire
 * process, and a given signal can have only one handler.  This
 * property makes it difficult for distinct components running in a
 * single process to cooperate.  While a newly-registered signal
 * handler can forward to a previously-installer handler (many
 * existing libraries using this approach), this technique requires a
 * strict nesting of handling installation and uninstallation, making
 * it impossible in practice to safely remove handlers and unload
 * components.
 *
 * libsigmux provides a central dispatch point for signal handling.
 * Components can register for signal handler processing and elect to
 * either handle a given signal or delegate to other handlers.
 * Components can register and unregister their handlers in arbitrary
 * order.
 *
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <setjmp.h>

#ifdef SIGMUX_BUILD_DSO
#define SIGMUX_EXPORT __attribute__((visibility("default")))
#else
#define SIGMUX_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct sigmux_registration;

enum sigmux_action {
  SIGMUX_CONTINUE_SEARCH,
  SIGMUX_CONTINUE_EXECUTION
};

struct sigmux_siginfo {
  int signum;
  siginfo_t* info;
  void* context;
};

/**
 * Handle callback for a signal.  See sigmux_add.
 */
typedef enum sigmux_action (*sigmux_handler)(
  struct sigmux_siginfo* siginfo,
  void* handler_data
);

/**
 * Initialize the library for use with signal signum.  On success,
 * return 0.  On error, return -1 with errno set.
 *
 * sigmux_init installs a signal handler with sigaction.  If a signal
 * handler is installed when sigmux_init is called, sigmux will invoke
 * that handler when no sigmux-registered handler opts to handle a
 * signal.  Do not manually install signal handlers for signum after
 * calling sigmux_init on signum.  The signal handler is installed
 * with SA_NODEFER | SA_ONSTACK | SA_RESTART.
 *
 * Once initialized, the sigmux library cannot be uninitialized or
 * removed from memory.
 *
 * sigmux_init is idempotent with respect to a given signal number.
 */
SIGMUX_EXPORT extern int sigmux_init(
  int signum
);

/**
 * Exit non-locally from a sigmux handler.  Use this routine instead
 * of longjmp or siglongjmp in order to restore internal sigmux
 * bookkeeping information.  SIGINFO is the signal information passed
 * to the current handler; BUF and VAL are as for regular siglongjmp.
 */
SIGMUX_EXPORT __attribute__((noreturn)) void sigmux_longjmp(
  struct sigmux_siginfo* siginfo,
  sigjmp_buf buf,
  int val
);

#define SIGMUX_LOW_PRIORITY (1<<0)

/**
 * Register a signal handler with sigmux.  Return a registration
 * cookie on success or NULL with errno set on error.
 *
 * SIGNALS is a set of signals for which to invoke HANDLER.
 * Registration occurs atomically for all signals.  HANDLER is a
 * function to call when a signal in SIGNALS arrives.  HANDLER_DATA is
 * passed to HANDLER along with signal-specific information.
 *
 * Handlers for a given signal are invoked in reverse order of
 * registration.
 *
 * When a signal arrives, sigmux invokes all registered handlers for
 * that signal until one returns SIGMUX_CONTINUE_EXECUTION.  If a
 * handler returns SIGMUX_CONTINUE_EXECUTION, sigmux returns normally
 * from its POSIX signal handler.  If no sigmux handler returns
 * SIGMUX_CONTINUE_EXECUTION, sigmux invokes the signal handler that
 * was in effect when sigmux_init was called on that signal.
 *
 * FLAGS contains zero or more of the SIGMUX_ flag constants above.
 *
 * SIGMUX_LOW_PRIORITY --- try the corresponding handler only after
 * all handlers that did not specify SIGMUX_LOW_PRIORITY returned
 * SIGMUX_CONTINUE_SEARCH.
 *
 * In rare cases, sigmux may re-run handlers that returned
 * SIGMUX_CONTINUE_SEARCH.
 *
 * The caller must have called sigmux_init on each signal number for
 * which a handler is being registered.
 */
SIGMUX_EXPORT struct sigmux_registration* sigmux_register(
  const sigset_t* signals,
  sigmux_handler handler,
  void* handler_data,
  unsigned flags
);

/**
 * Unregister a signal handler registered with sigmux_register.
 * REGISTRATION_COOKIE is the value sigmux_register returned.
 */
SIGMUX_EXPORT void sigmux_unregister(
  struct sigmux_registration* registration_cookie
);

/**
 * Like sigaction(2), but operates on the next-signal handlers instead
 * of the current one.  This routine is useful either as a drop-in
 * replacement for sigaction(2) or as a forced replacement for
 * sigaction emplaced with LD_PRELOAD or a function hooking library
 * like distract.  Use sigmux_set_real_sigaction to tell sigmux the
 * function to call instead of sigaction.
 */
SIGMUX_EXPORT int sigmux_sigaction(int signum,
                                   const struct sigaction* act,
                                   struct sigaction* oldact);

/**
 * Sigmux sigaction implementation.
 */
typedef int (*sigmux_sigaction_function)(
  int signum,
  const struct sigaction* act,
  struct sigaction* oldact);

#define SIGMUX_SIGACTION_DEFAULT NULL

/**
 * Tell sigmux to call REAL_SIGACTION intead of the default
 * sigaction(2).  This routine is useful then using a hooking facility
 * to redirect the default operating system sigaction(2) to
 * sigmux_sigaction.  If SIGMUX_SIGACTION_DEFAULT is specified, call
 * the default function.  Return the previous sigaction handler.
 *
 * N.B. We provide this method instead of just using the rt_sigaction
 * system call so that we properly integrate with someone who's hooked
 * sigaction before we did, e.g., libsigchain in Android's
 * ART runtime.
 */
SIGMUX_EXPORT sigmux_sigaction_function sigmux_set_real_sigaction(
  sigmux_sigaction_function real_sigaction);

/**
 * Consider handlers registered with normal priority.
 */
#define SIGMUX_HANDLE_SIGNAL_NORMAL_PRIORITY (1<<0)

/**
 * Consider handlers registered at low priority
 */
#define SIGMUX_HANDLE_SIGNAL_LOW_PRIORITY (1<<1)

/**
 * Invoke the original signal handler
 */
#define SIGMUX_HANDLE_SIGNAL_INVOKE_DEFAULT (1<<2)

/**
 * Signal handler exposed directly; useful for callers that hook other
 * signal handler wrappers.  SIGNUM, INFO, and CONTEXT are as for
 * siginfo(2).  FLAGS tells us how to handle the signal and is zero or
 * more of the SIGMUX_HANDLE_SIGNAL_* constants.  Return what we did
 * with the signal.
 */
SIGMUX_EXPORT enum sigmux_action sigmux_handle_signal(
  int signum,
  siginfo_t* info,
  void* context,
  int flags);


#ifdef __cplusplus
} // extern "C"
#endif
