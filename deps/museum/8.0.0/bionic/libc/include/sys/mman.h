/*
 * Copyright (C) 2008 The Android Open Source Project
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
#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <asm/mman.h>

__BEGIN_DECLS

#ifndef MAP_ANON
#define MAP_ANON  MAP_ANONYMOUS
#endif

#define MAP_FAILED __BIONIC_CAST(reinterpret_cast, void*, -1)

#define MREMAP_MAYMOVE  1
#define MREMAP_FIXED    2

#define POSIX_MADV_NORMAL     MADV_NORMAL
#define POSIX_MADV_RANDOM     MADV_RANDOM
#define POSIX_MADV_SEQUENTIAL MADV_SEQUENTIAL
#define POSIX_MADV_WILLNEED   MADV_WILLNEED
#define POSIX_MADV_DONTNEED   MADV_DONTNEED

#if defined(__USE_FILE_OFFSET64) && __ANDROID_API__ >= __ANDROID_API_L__
void* mmap(void*, size_t, int, int, int, off_t) __RENAME(mmap64) __INTRODUCED_IN(21);
#else
void* mmap(void*, size_t, int, int, int, off_t);
#endif
void* mmap64(void*, size_t, int, int, int, off64_t) __INTRODUCED_IN(21);

int munmap(void*, size_t);
int msync(void*, size_t, int);
int mprotect(void*, size_t, int);
void* mremap(void*, size_t, size_t, int, ...);

int mlockall(int) __INTRODUCED_IN(17);
int munlockall(void) __INTRODUCED_IN(17);
int mlock(const void*, size_t);
int munlock(const void*, size_t);

int mincore(void*, size_t, unsigned char*);

int madvise(void*, size_t, int);
int posix_madvise(void*, size_t, int) __INTRODUCED_IN(23);

__END_DECLS

#endif /* _SYS_MMAN_H_ */
