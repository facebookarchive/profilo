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

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>

#include "sigmux.h"
#include "phaser.h"

#ifndef USE_RT_SIGPROCMASK
# if defined(__linux__) && defined(__ANDROID__)
#  define USE_RT_SIGPROCMASK 1
# else
#  define USE_RT_SIGPROCMASK 0
# endif
#endif

#if USE_RT_SIGPROCMASK
# include <linux_syscall_support.h>
#endif

#define ARRAY_SIZE(a) ((sizeof ((a)) / (sizeof ((a)[0]))))
static inline bool verify_dummy(bool b) {return b;}
#ifdef NDEBUG
#define VERIFY(e) verify_dummy((e))
#else
#define VERIFY(e) (assert((e)), verify_dummy(true))
#endif

// Cast from part of a structure to the whole thing.  Identical to the
// Linux kernel container_of macro.
#define container_of(ptr, type, member) ({                      \
      const typeof( ((type *)0)->member ) *__mptr = (ptr);      \
      (type *)( (char *)__mptr - offsetof(type,member) );       \
    })

struct sigmux_siginfo_internal {
  struct sigmux_siginfo public;
  phaser_phase phase;
};

// N.B. concurrent readers may only iterate over the list in the
// forward direction.  Access to prev pointers requires callers to
// hold the modification mutex.

struct sigmux_registration_link {
  struct sigmux_registration_link* prev;
  struct sigmux_registration_link* next;
};

struct sigmux_registration {
  struct sigmux_registration_link link;
  sigset_t signals;
  sigmux_handler handler;
  void* handler_data;
  unsigned flags;
};

// Use uint8_t and own bit-set structure instead of sigset_t for
// sigmux_global.initsig.  This way, the debugger doesn't have to
// understand the implementation of sigset_t.
struct sigmux_sigset {
  uint8_t s[(NSIG+7)/8];
};

// N.B. Not static --- we want to be able to find this symbol using
// gdb.lookup_global_symbol.  We use -fvisibility=hidden, so this
// symbol still isn't exposed to other DSOs.
struct sigmux_global {
  pthread_mutex_t lock;
  phaser_t phaser;
  int phaser_needs_init;
  struct sigaction* orig_sigact[NSIG];
  struct sigaction* alt_sigact[NSIG];
  struct sigmux_registration_link handlers;
  sigmux_sigaction_function real_sigaction;
  struct sigmux_sigset initsig;
} sigmux_global = {
  .lock = PTHREAD_MUTEX_INITIALIZER,
  .phaser = PHASER_UNINITIALIZED,
  .phaser_needs_init = 1,
  .handlers = { &sigmux_global.handlers, &sigmux_global.handlers },
};

#ifdef __ANDROID__
static const sigset_t no_signals;
#endif

static int
invoke_real_sigaction(int signum,
                 const struct sigaction* act,
                 struct sigaction* oldact)
{
  return (sigmux_global.real_sigaction ?: sigaction)(signum, act, oldact);
}

static int
sigmux_sigismember(const struct sigmux_sigset* ss, int signum)
{
  if (!(0 < signum && signum < NSIG)) {
    errno = EINVAL;
    return -1;
  }

  return !!(ss->s[signum/8] & (1<<(signum % 8)));
}

static void
sigmux_sigaddset(struct sigmux_sigset* ss, int signum)
{
  ss->s[signum/8] |= (1<<(signum % 8));
}

/**
 * Is this signal fatal when configured for SIG_DFL?
 */
static bool
signal_default_fatal_p(int signum)
{
  switch (signum) {
    case SIGCHLD:
    case SIGWINCH:
      return false;
    default:
      return true;
  }
}

/**
 * Is this signal fatal when configured for SIG_IGN?
 */
static bool
signal_always_fatal_p(int signum)
{
  switch (signum) {
    case SIGSEGV:
    case SIGBUS:
    case SIGABRT:
    case SIGILL:
      return true;
    default:
      return false;
  }
}

/* Clang does not have noclone. */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
#endif
/**
 * The debugger sets a breakpoint here to run code when sigmux has run
 * out of signal-handling options and is about to die horribly.
 */
__attribute__((noinline,noclone))
void // Not static!
sigmux_gdbhook_on_fatal_signal(siginfo_t* info, void* context)
{
  /* Noop: debugger sets breakpoint here */
  (void) info;
  (void) context;
  asm volatile ("");
}

/**
 * The debugger sets a breakpoint here to run code after sigmux has
 * taken over responsibility for a signal.
 */
__attribute__((noinline,noclone))
void // Not static!
sigmux_gdbhook_on_signal_seized(void)
{
  /* Noop: debugger sets breakpoint here */
  asm volatile ("");
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

static void
set_signal_handler_to_default(int signum)
{
#if USE_RT_SIGPROCMASK
  // sigchain has a bug in Android 5.0.x where it ignores attempts to
  // reset to SIG_DFL; just use the system call directly in this case.
  struct kernel_sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sys_sigemptyset(&sa.sa_mask);
  sa.sa_handler_ = SIG_DFL;
  sa.sa_flags = SA_RESTART;
  sys_rt_sigaction(signum, &sa, NULL, sizeof(struct kernel_sigset_t));
#else
  struct sigaction sa;
  memset(&sa, 0, sizeof (sa));
  sa.sa_handler = SIG_DFL;
  sa.sa_flags = SA_RESTART;
  invoke_real_sigaction(signum, &sa, NULL);
#endif
}

static void
set_sigmask_for_handler(const struct sigaction* action, int signum)
{
  sigset_t new_mask;
  memcpy(&new_mask, &action->sa_mask, sizeof (sigset_t));
  if ((action->sa_flags & SA_NODEFER) == 0) {
    sigaddset(&new_mask, signum);
  }
  sigprocmask(SIG_SETMASK, &new_mask, NULL);
}

static void
invoke_sighandler(const struct sigaction* action,
                  int signum,
                  siginfo_t* info,
                  void* context)
{
  if (signal_always_fatal_p(signum)) {
    sigmux_gdbhook_on_fatal_signal(info, context);
  }

  // N.B. We don't need to restore the signal mask, since returning
  // normally from the signal handler will do it for us.  If the
  // signal handler returns non-locally, it has the burden of
  // resetting the signal mask whether it's being called by the kernel
  // directly or by us.
  //
  // Also note that the default action of any signal is to either do
  // nothing, bring down the process, or stop the process.

  bool is_siginfo = (action->sa_flags & SA_SIGINFO);
  bool is_default = is_siginfo
    ? action->sa_sigaction == NULL
    : action->sa_handler == SIG_DFL;
  bool is_ignore = !is_siginfo && action->sa_handler == SIG_IGN;

  if (is_siginfo && !is_default && !is_ignore) {
    set_sigmask_for_handler(action, signum);
    action->sa_sigaction(signum, info, context);
  } else if (!is_siginfo && !is_default && !is_ignore) {
    set_sigmask_for_handler(action, signum);
    action->sa_handler(signum);
  } else if (signal_always_fatal_p(signum) ||
             (is_default && signal_default_fatal_p(signum)))
  {
    sigset_t to_unblock;
    set_signal_handler_to_default(signum);
    sigemptyset(&to_unblock);
    sigaddset(&to_unblock, signum);
    sigprocmask(SIG_UNBLOCK, &to_unblock, NULL);
    raise(signum);
    abort();
  } else if (is_default && (signum == SIGTSTP ||
                            signum == SIGTTIN ||
                            signum == SIGTTOU))
  {
    raise(SIGSTOP);
  }
}

void
sigmux_longjmp(
  struct sigmux_siginfo* public_siginfo,
  sigjmp_buf buf,
  int val)
{
  struct sigmux_siginfo_internal* siginfo =
    container_of(public_siginfo,
                 struct sigmux_siginfo_internal,
                 public);

  phaser_exit(&sigmux_global.phaser, siginfo->phase);
  siglongjmp(buf, val);
}

enum sigmux_action
sigmux_handle_signal(
  int signum,
  siginfo_t* info,
  void* context,
  int flags)
{
  struct sigmux_registration* reg;
  struct sigmux_registration_link* it;
  struct sigmux_registration_link* reglist;

  enum sigmux_action action;
  struct sigmux_siginfo_internal siginfo = {
    .public = {
      .signum = signum,
      .info = info,
      .context = context,
    },

    .phase = phaser_enter(&sigmux_global.phaser),
  };

  // __ATOMIC_RELAXED is fine: phaser_enter is a memory barrier.
  reglist = __atomic_load_n(&sigmux_global.handlers.next, __ATOMIC_RELAXED);
  action = SIGMUX_CONTINUE_SEARCH;

  if ((flags & SIGMUX_HANDLE_SIGNAL_NORMAL_PRIORITY)) {
    for (it = reglist;
         it != &sigmux_global.handlers && action == SIGMUX_CONTINUE_SEARCH;
         it = it->next)
    {
      reg = container_of(it, struct sigmux_registration, link);
      if ((reg->flags & SIGMUX_LOW_PRIORITY) == 0 &&
          sigismember(&reg->signals, signum) == 1)
      {
        action = reg->handler(&siginfo.public, reg->handler_data);
      }
    }
  }

  if ((flags & SIGMUX_HANDLE_SIGNAL_LOW_PRIORITY)) {
    for (it = reglist;
         it != &sigmux_global.handlers && action == SIGMUX_CONTINUE_SEARCH;
         it = it->next)
    {
      reg = container_of(it, struct sigmux_registration, link);
      if ((reg->flags & SIGMUX_LOW_PRIORITY) != 0 &&
          sigismember(&reg->signals, signum) == 1)
      {
        action = reg->handler(&siginfo.public, reg->handler_data);
      }
    }
  }

  // We need to copy the next handler to local storage _before_ we end
  // the phase, then use this local storage in invoke_sighandler.
  // If we just used the default sighandler directly, we'd race with
  // concurrent callers to sigmux_sigaction.  We can't just end the
  // phase after we call invoke_sighandler, because invoke_sighandler
  // may return non-locally.  For the same reason, we can't just
  // protect orig_sigact with a lock.

  struct sigaction next_handler;

  if ((flags & SIGMUX_HANDLE_SIGNAL_INVOKE_DEFAULT) &&
      action == SIGMUX_CONTINUE_SEARCH) {
    struct sigaction* next_handler_snapshot =
      __atomic_load_n(&sigmux_global.orig_sigact[signum], __ATOMIC_CONSUME);
    next_handler = *next_handler_snapshot;
    // For one-shot signal handlers, we execute the action only once,
    // so let threads compete to see who can set sa_handler (or
    // sa_sigaction) to NULL first.  If we win, the CAS succeeds and
    // sa_handler (or sa_sigaction) ends up with the handler to
    // execute.  If we lose, it ends up with SIG_DFL (or NULL), which
    // tells us to ignore the signal in invoke_sighandler.
    if (next_handler.sa_flags & SA_RESETHAND) {
      if (next_handler.sa_flags & SA_SIGINFO) {
        (void) __atomic_compare_exchange_n(&next_handler_snapshot->sa_handler,
                                           &next_handler.sa_handler,
                                           SIG_DFL,
                                           true,
                                           __ATOMIC_RELAXED,
                                           __ATOMIC_RELAXED);
      } else {
        (void) __atomic_compare_exchange_n(&next_handler_snapshot->sa_sigaction,
                                           &next_handler.sa_sigaction,
                                           NULL,
                                           true,
                                           __ATOMIC_RELAXED,
                                           __ATOMIC_RELAXED);

      }
      // Don't bother going through this process next time.
      next_handler_snapshot->sa_flags &= ~SA_RESETHAND;
    }
  }

  phaser_exit(&sigmux_global.phaser, siginfo.phase);

  if ((flags & SIGMUX_HANDLE_SIGNAL_INVOKE_DEFAULT) &&
      action == SIGMUX_CONTINUE_SEARCH) {
    invoke_sighandler(&next_handler, signum, info, context);
    action = SIGMUX_CONTINUE_EXECUTION;
  }

  return action;
}

static void
sigmux_handle_signal_1(
  int signum,
  siginfo_t* info,
  void* context)
{
  int orig_errno = errno;
#ifdef __ANDROID__
  // Depending on Android version, sigchain can call us with any
  // random signal mask set despite our asking for no blocked signals
  // and our using SA_NODEFER.  Reset the signal mask explicitly.
  sigprocmask(SIG_SETMASK, &no_signals, NULL);
#endif
  (void) sigmux_handle_signal(
    signum,
    info,
    context,
    ( SIGMUX_HANDLE_SIGNAL_NORMAL_PRIORITY |
      SIGMUX_HANDLE_SIGNAL_LOW_PRIORITY |
      SIGMUX_HANDLE_SIGNAL_INVOKE_DEFAULT ));
  errno = orig_errno;
}

static struct sigaction*
allocate_sigaction(struct sigaction** sap)
{
  if (*sap == NULL) {
    *sap = calloc(1, sizeof (**sap));
  }
  return *sap;
}

int
sigmux_init(int signum)
{
  int ismem;
  struct sigaction newact;
  int ret = -1;

  VERIFY(0 == pthread_mutex_lock(&sigmux_global.lock));
  if (sigmux_global.phaser_needs_init) {
    if (phaser_init(&sigmux_global.phaser) != 0) {
      goto out;
    }
    sigmux_global.phaser_needs_init = 0;
  }
  ismem = sigmux_sigismember(&sigmux_global.initsig, signum);
  if (ismem == -1) {
    goto out;
  }

  if (ismem == 0) {

    struct sigaction* orig_sigact =
      allocate_sigaction(&sigmux_global.orig_sigact[signum]);
    if (orig_sigact == NULL) {
      goto out;
    }

    // Pre-allocate spare memory for sigmux_sigaction, since it isn't
    // allowed to fail with ENOMEM.
    if (allocate_sigaction(&sigmux_global.alt_sigact[signum]) == NULL) {
      goto out;
    }

    memset(&newact, 0, sizeof (newact));
    newact.sa_sigaction = sigmux_handle_signal_1;
    newact.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK | SA_RESTART;
    if (invoke_real_sigaction(signum, &newact, orig_sigact) != 0) {
      goto out;
    }

    sigmux_sigaddset(&sigmux_global.initsig, signum);
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    sigmux_gdbhook_on_signal_seized();
  }

  ret = 0;

  out:

  VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));
  return ret;
}

int
sigmux_reinit(int signum, int flags)
{
  int ismem;
  struct sigaction reinitact;
  int ret = -1;

  VERIFY(0 == pthread_mutex_lock(&sigmux_global.lock));

  ismem = sigmux_sigismember(&sigmux_global.initsig, signum);
  if (ismem == -1) {
    goto out;
  }

  // Not inited
  if (ismem == 0) {
    goto out;
  }

  struct sigaction* orig_sigaction_tmp =
    (flags & RESET_ORIG_SIGACTION_FLAG) != 0
         ? calloc(1, sizeof(struct sigaction))
         : NULL;

  memset(&reinitact, 0, sizeof (reinitact));
  reinitact.sa_sigaction = sigmux_handle_signal_1;
  reinitact.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK | SA_RESTART;
  if (invoke_real_sigaction(signum, &reinitact, orig_sigaction_tmp) != 0) {
    goto out;
  }

  // Only reset the original sigaction if were asked to
  if (orig_sigaction_tmp != NULL) {
    struct sigaction* old_orig_sigaction = sigmux_global.orig_sigact[signum];
    sigmux_global.orig_sigact[signum] = orig_sigaction_tmp;
    free(old_orig_sigaction);
  }

  __atomic_signal_fence(__ATOMIC_SEQ_CST);
  sigmux_gdbhook_on_signal_seized();

  ret = 0;

  out:

  VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));
  return ret;
}

struct sigmux_registration*
sigmux_register(
  const sigset_t* signals,
  sigmux_handler handler,
  void* handler_data,
  unsigned flags)
{
  struct sigmux_registration* reg;

  reg = calloc(1, sizeof (*reg));
  if (reg == NULL) {
    return NULL;
  }

  reg->signals = *signals;
  reg->handler = handler;
  reg->handler_data = handler_data;
  reg->flags = flags;

  VERIFY(0 == pthread_mutex_lock(&sigmux_global.lock));

  // Atomically prepend our handler to the list.  We perform all
  // modification to the list under sigmux_global.lock, so we need
  // only worry about concurrent readers of
  // sigmux_global.handlers.next, who will see either the old
  // sigmux_global.handlers.next or our new one.  __ATOMIC_RELEASE
  // ensures readers see only fully-constructed object.

  reg->link.next = sigmux_global.handlers.next;
  reg->link.prev = &sigmux_global.handlers;
  reg->link.next->prev = &reg->link;
  __atomic_store_n(&sigmux_global.handlers.next, &reg->link, __ATOMIC_RELEASE);

  VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));

  return reg;
}

void
sigmux_unregister(struct sigmux_registration* registration_cookie)
{
  struct sigmux_registration* reg = registration_cookie;

  // Make concurrent readers bypass the handler we're trying to
  // unregister.  Wait for all active readers to complete.  The
  // phaser_drain is a memory barrier, so our write to reg->prev will
  // be visible.
  VERIFY(0 == pthread_mutex_lock(&sigmux_global.lock));
  reg->link.prev->next = reg->link.next;
  reg->link.next->prev = reg->link.prev;
  phaser_drain(&sigmux_global.phaser);
  VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));
  free(reg);
}

int
sigmux_sigaction(int signum,
                 const struct sigaction* act,
                 struct sigaction* oldact)
{
  VERIFY(0 == pthread_mutex_lock(&sigmux_global.lock));

  if (!sigmux_sigismember(&sigmux_global.initsig, signum)
      || signum <= 0 || signum >= NSIG)
  {
    // We're not hooked, so just defer to the original sigaction.
    // Make sure to release the lock before we do so that if
    // real_sigaction is some kind of weird thing that ends up calling
    // back into us, we don't deadlock.
    VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));
    return invoke_real_sigaction(signum, act, oldact);
  }

  // sigaction(2) is technically isn't allowed to crash if ACT or
  // OLDACT points to invalid memory (it's supposed to fail with
  // EFAULT instead), and we do, but this particular spec violation
  // probably doesn't matter.  (We could use mincore(2) to test the
  // memory, but we could still race with an unmap or mprotect.)

  if (oldact != NULL) {
    *oldact = *sigmux_global.orig_sigact[signum];

    // If the current handler is a one-shot handler, it may be in an
    // invalid state as a result of how we use atomic CAS to implement
    // SA_RESETHAND.  In this case, munge the returned struct
    // sigaction to make it look like we atomically reset the
    // whole thing.

    bool pretend_default =
      ((oldact->sa_flags & SA_SIGINFO) && oldact->sa_sigaction == NULL) ||
      (!(oldact->sa_flags & SA_SIGINFO) && oldact->sa_handler == SIG_DFL);

    if (pretend_default) {
      oldact->sa_flags &= ~(SA_RESETHAND | SA_SIGINFO);
      oldact->sa_handler = SIG_DFL;
    }
  }

  if (act != NULL) {
    *sigmux_global.alt_sigact[signum] = *act;
    __atomic_exchange(&sigmux_global.orig_sigact[signum],
                      &sigmux_global.alt_sigact[signum],
                      &sigmux_global.alt_sigact[signum],
                      __ATOMIC_RELEASE);
    phaser_drain(&sigmux_global.phaser);
  }

  VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));
  return 0;
}


sigmux_sigaction_function
sigmux_set_real_sigaction(sigmux_sigaction_function real_sigaction)
{
  sigmux_sigaction_function old_sigaction;
  VERIFY(0 == pthread_mutex_lock(&sigmux_global.lock));
  old_sigaction = sigmux_global.real_sigaction;
  sigmux_global.real_sigaction = real_sigaction;
  VERIFY(0 == pthread_mutex_unlock(&sigmux_global.lock));
  return old_sigaction;
}
