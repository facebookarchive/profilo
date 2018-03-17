/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BIONIC_OPENBSD_COMPAT_H_included
#define _BIONIC_OPENBSD_COMPAT_H_included

#include <sys/cdefs.h>
#include <stddef.h> // For size_t.

#define __USE_BSD

/* OpenBSD's <ctype.h> uses these names, which conflicted with stlport.
 * Additionally, we changed the numeric/digit type from N to D for libcxx.
 */
#define _U _CTYPE_U
#define _L _CTYPE_L
#define _N _CTYPE_D
#define _S _CTYPE_S
#define _P _CTYPE_P
#define _C _CTYPE_C
#define _X _CTYPE_X
#define _B _CTYPE_B

/* OpenBSD has this, but we can't really implement it correctly on Linux. */
#define issetugid() 0

#define explicit_bzero(p, s) memset(p, 0, s)

/* We have OpenBSD's getentropy_linux.c, but we don't mention getentropy in any header. */
__LIBC_HIDDEN__ extern int getentropy(void*, size_t);

/* LP32 NDK ctype.h contained references to these. */
__LIBC64_HIDDEN__ extern const short* _tolower_tab_;
__LIBC64_HIDDEN__ extern const short* _toupper_tab_;

__LIBC_HIDDEN__ extern struct atexit* __atexit;
__LIBC_HIDDEN__ extern const char _C_ctype_[];
__LIBC_HIDDEN__ extern const short _C_toupper_[];
__LIBC_HIDDEN__ extern const short _C_tolower_[];
__LIBC_HIDDEN__ extern char* __findenv(const char*, int, int*);
__LIBC_HIDDEN__ extern char* _mktemp(char*);

/* TODO: hide this when android_support.a is fixed (http://b/16298580).*/
/*__LIBC_HIDDEN__*/ extern int __isthreaded;

#endif
