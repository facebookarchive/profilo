/*
 * Copyright (C) 2010 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ASYNC_SAFE_LOG_LOG_H
#define _ASYNC_SAFE_LOG_LOG_H

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// These functions do not allocate memory to send data to the log.

__BEGIN_DECLS

enum {
  ANDROID_LOG_UNKNOWN = 0,
  ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */

  ANDROID_LOG_VERBOSE,
  ANDROID_LOG_DEBUG,
  ANDROID_LOG_INFO,
  ANDROID_LOG_WARN,
  ANDROID_LOG_ERROR,
  ANDROID_LOG_FATAL,

  ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
};

enum {
  LOG_ID_MIN = 0,

  LOG_ID_MAIN = 0,
  LOG_ID_RADIO = 1,
  LOG_ID_EVENTS = 2,
  LOG_ID_SYSTEM = 3,
  LOG_ID_CRASH = 4,

  LOG_ID_MAX
};

// Formats a message to the log (priority 'fatal'), then aborts.
// Implemented as a macro so that async_safe_fatal isn't on the stack when we crash:
// we appear to go straight from the caller to abort, saving an uninteresting stack
// frame.
#define async_safe_fatal(...) \
  do { \
    async_safe_fatal_no_abort(__VA_ARGS__); \
    abort(); \
  } while (0) \


// These functions do return, so callers that want to abort, must do so themselves,
// or use the macro above.
void async_safe_fatal_no_abort(const char* _Nonnull fmt, ...) __printflike(1, 2);
#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__)
void async_safe_fatal_va_list(
    const char* _Nullable prefix, const char* _Nonnull fmt, va_list);
#else // defined(__mips__) || defined(__i386__)
void async_safe_fatal_va_list(
    const char* _Nullable prefix, const char* _Nonnull fmt, va_list _Nonnull);
#endif

//
// Formatting routines for the C library's internal debugging.
// Unlike the usual alternatives, these don't allocate, and they don't drag in all of stdio.
// These are async signal safe, so they can be called from signal handlers.
//

int async_safe_format_buffer(char* _Nonnull buf, size_t size, const char* _Nonnull fmt, ...) __printflike(3, 4);

#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__)
int async_safe_format_buffer_va_list(
    char* _Nonnull buffer, size_t buffer_size, const char* _Nonnull format, va_list args);
#else // defined(__mips__) || defined(__i386__)
int async_safe_format_buffer_va_list(
    char* _Nonnull buffer, size_t buffer_size, const char* _Nonnull format, va_list _Nonnull args);
#endif

int async_safe_format_fd(int fd, const char* _Nonnull format , ...) __printflike(2, 3);
int async_safe_format_log(int pri, const char* _Nonnull tag, const char* _Nonnull fmt, ...) __printflike(3, 4);
#if defined(__arm__) || defined(__aarch64__) || defined(__x86_64__)
int async_safe_format_log_va_list(
    int pri, const char* _Nonnull tag, const char* _Nonnull fmt, va_list ap);
#else // defined(__mips__) || defined(__i386__)
int async_safe_format_log_va_list(
    int pri, const char* _Nonnull tag, const char* _Nonnull fmt, va_list _Nonnull ap);
#endif
int async_safe_write_log(int pri, const char* _Nonnull tag, const char* _Nonnull msg);

#define CHECK(predicate) \
  do { \
    if (!(predicate)) { \
      async_safe_fatal("%s:%d: %s CHECK '" #predicate "' failed", \
          __FILE__, __LINE__, __FUNCTION__); \
    } \
  } while(0)

__END_DECLS

#endif
