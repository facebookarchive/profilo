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

#include "sig_safe_write.h"

#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <sigmux.h>

struct fault_handler_data {
  int tid;
  int active;
  int check_sigill;
  jmp_buf jump_buffer;
};

/**
 * Returns an integer uniquely identifying current thread.
 * This function is async-signal-safe.
 */
static int
as_safe_gettid()
{
  return syscall(__NR_gettid);
}

/**
 * Signal handler that jumps out of the handler after determining the signal is
 * caused by executing predetermined code on predetermined thread.
 */
static enum sigmux_action
fault_handler(
  struct sigmux_siginfo* siginfo,
  void* handler_data)
{
  struct fault_handler_data* data = (struct fault_handler_data*) handler_data;
  siginfo_t* info = siginfo->info;

  if (__atomic_load_n(&data->tid, __ATOMIC_SEQ_CST) != as_safe_gettid()) {
    return SIGMUX_CONTINUE_SEARCH;
  }

  if (!__atomic_load_n(&data->active, __ATOMIC_SEQ_CST)) {
    return SIGMUX_CONTINUE_SEARCH;
  }

  if (__atomic_load_n(&data->check_sigill, __ATOMIC_SEQ_CST)) {
    /* Expect SIGILL signal */
    if (info->si_signo != SIGILL) {
      return SIGMUX_CONTINUE_SEARCH;
    }
  } else {
    /* Expect SIGSEGV or SIGBUS signal */
    if (info->si_signo != SIGSEGV && info->si_signo != SIGBUS) {
      return SIGMUX_CONTINUE_SEARCH;
    }
  }

  sigmux_longjmp(siginfo, data->jump_buffer, 1);

  // not reached
  return SIGMUX_CONTINUE_EXECUTION;
}

int
sig_safe_op (void (*op)(void*), void* data)
{
  struct fault_handler_data handler_data = {};
  struct sigmux_registration* registration = NULL;
  int result = 1;

  __atomic_store_n(&handler_data.tid, as_safe_gettid(), __ATOMIC_SEQ_CST);
  __atomic_store_n(&handler_data.check_sigill, 0, __ATOMIC_SEQ_CST);

  sigset_t sigset;
  if (sigemptyset(&sigset) ||
      sigaddset(&sigset, SIGSEGV) ||
      sigaddset(&sigset, SIGBUS))
  {
    goto out;
  }

  registration = sigmux_register(&sigset, &fault_handler, &handler_data, 0);
  if (!registration) {
    goto out;
  }

  // Must be after `registration` is valid. If we ever longjmp to the buffer
  // below, we will need the actual value in order to sigmux_unregister it.
  if (sigsetjmp(handler_data.jump_buffer, 1)) {
    errno = EFAULT;
    goto out;
  }

  __atomic_store_n(&handler_data.active, 1, __ATOMIC_SEQ_CST);
  op(data);
  __atomic_store_n(&handler_data.active, 0, __ATOMIC_SEQ_CST);

  result = 0;

out:
  if (registration) {
    int old_errno = errno;
    sigmux_unregister(registration);
    errno = old_errno;
  }

  return result;
}

int
sig_safe_exec (void (*op)(void*), void* data)
{
  struct fault_handler_data handler_data = {};
  struct sigmux_registration* registration = NULL;
  int result = 1;

  __atomic_store_n(&handler_data.tid, as_safe_gettid(), __ATOMIC_SEQ_CST);
  __atomic_store_n(&handler_data.check_sigill, 1, __ATOMIC_SEQ_CST);

  if (sigsetjmp(handler_data.jump_buffer, 1)) {
    errno = EFAULT;
    goto out;
  }

  sigset_t sigset;
  if (sigemptyset(&sigset) ||
      sigaddset(&sigset, SIGILL))
  {
    goto out;
  }

  registration = sigmux_register(&sigset, &fault_handler, &handler_data, 0);
  if (!registration) {
    goto out;
  }

  __atomic_store_n(&handler_data.active, 1, __ATOMIC_SEQ_CST);
  op(data);
  __atomic_store_n(&handler_data.active, 0, __ATOMIC_SEQ_CST);

  result = 0;

out:
  if (registration) {
    int old_errno = errno;
    sigmux_unregister(registration);
    errno = old_errno;
  }

  return result;
}

struct write_params {
  void* destination;
  intptr_t value;
};

static void
sig_safe_write_op(void* data)
{
  struct write_params* params = (struct write_params*) data;

  __atomic_store_n(
    (intptr_t*)params->destination,
    params->value,
    __ATOMIC_SEQ_CST);
}

int
sig_safe_write(void* destination, intptr_t value)
{
  struct write_params params = {
    .destination = destination,
    .value = value,
  };

  return sig_safe_op(&sig_safe_write_op, &params);
}
