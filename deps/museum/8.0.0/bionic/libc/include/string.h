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

#ifndef _STRING_H
#define _STRING_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <xlocale.h>

#include <bits/strcasecmp.h>

__BEGIN_DECLS

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif

#if defined(__USE_BSD)
#include <strings.h>
#endif

void* memccpy(void* _Nonnull __restrict, const void* _Nonnull __restrict, int, size_t);
void* memchr(const void* _Nonnull, int, size_t) __attribute_pure__ __overloadable
        __RENAME_CLANG(memchr);
void* memrchr(const void* _Nonnull, int, size_t) __attribute_pure__ __overloadable
        __RENAME_CLANG(memrchr);
int memcmp(const void* _Nonnull, const void* _Nonnull, size_t) __attribute_pure__;
void* memcpy(void* _Nonnull __restrict, const void* _Nonnull __restrict, size_t)
        __overloadable __RENAME_CLANG(memcpy);
#if defined(__USE_GNU)
void* mempcpy(void* _Nonnull __restrict, const void* _Nonnull __restrict, size_t) __INTRODUCED_IN(23);
#endif
void* memmove(void* _Nonnull, const void* _Nonnull, size_t) __overloadable
        __RENAME_CLANG(memmove);
void* memset(void* _Nonnull, int, size_t) __overloadable __RENAME_CLANG(memset);
void* memmem(const void* _Nonnull, size_t, const void* _Nonnull, size_t) __attribute_pure__;

char* strchr(const char* _Nonnull, int) __attribute_pure__ __overloadable
        __RENAME_CLANG(strchr);
char* __strchr_chk(const char* _Nonnull, int, size_t) __INTRODUCED_IN(18);
#if defined(__USE_GNU)
#if defined(__cplusplus)
extern "C++" char* strchrnul(char* _Nonnull, int) __RENAME(strchrnul) __attribute_pure__;
extern "C++" const char* strchrnul(const char* _Nonnull, int) __RENAME(strchrnul) __attribute_pure__;
#else
char* strchrnul(const char* _Nonnull, int) __attribute_pure__ __INTRODUCED_IN(24);
#endif
#endif

char* strrchr(const char* _Nonnull, int) __attribute_pure__ __overloadable
        __RENAME_CLANG(strrchr);
char* __strrchr_chk(const char* _Nonnull, int, size_t) __INTRODUCED_IN(18);

size_t strlen(const char* _Nonnull) __attribute_pure__ __overloadable
        __RENAME_CLANG(strlen);
size_t __strlen_chk(const char* _Nonnull, size_t) __INTRODUCED_IN(17);

int strcmp(const char* _Nonnull, const char* _Nonnull) __attribute_pure__;
char* stpcpy(char* _Nonnull __restrict, const char* _Nonnull __restrict)
        __overloadable __RENAME_CLANG(stpcpy) __INTRODUCED_IN(21);
char* strcpy(char* _Nonnull __restrict, const char* _Nonnull __restrict)
        __overloadable __RENAME_CLANG(strcpy);
char* strcat(char* _Nonnull __restrict, const char* _Nonnull __restrict)
        __overloadable __RENAME_CLANG(strcat);
char* strdup(const char* _Nonnull);

char* strstr(const char* _Nonnull, const char* _Nonnull) __attribute_pure__;
char* strcasestr(const char* _Nonnull, const char* _Nonnull) __attribute_pure__;
char* strtok(char* __restrict, const char* _Nonnull __restrict);
char* strtok_r(char* __restrict, const char* _Nonnull __restrict, char** _Nonnull __restrict);

char* strerror(int);
char* strerror_l(int, locale_t) __INTRODUCED_IN(23);
#if defined(__USE_GNU) && __ANDROID_API__ >= 23
char* strerror_r(int, char*, size_t) __RENAME(__gnu_strerror_r) __INTRODUCED_IN(23);
#else /* POSIX */
int strerror_r(int, char*, size_t);
#endif

size_t strnlen(const char* _Nonnull, size_t) __attribute_pure__;
char* strncat(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t)
        __overloadable __RENAME_CLANG(strncat);
char* strndup(const char* _Nonnull, size_t);
int strncmp(const char* _Nonnull, const char* _Nonnull, size_t) __attribute_pure__;
char* stpncpy(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t)
        __overloadable __RENAME_CLANG(stpncpy) __INTRODUCED_IN(21);
char* strncpy(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t)
        __overloadable __RENAME_CLANG(strncpy);

size_t strlcat(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t)
        __overloadable __RENAME_CLANG(strlcat);
size_t strlcpy(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t)
        __overloadable __RENAME_CLANG(strlcpy);

size_t strcspn(const char* _Nonnull, const char* _Nonnull) __attribute_pure__;
char* strpbrk(const char* _Nonnull, const char* _Nonnull) __attribute_pure__;
char* strsep(char** _Nonnull __restrict, const char* _Nonnull __restrict);
size_t strspn(const char* _Nonnull, const char* _Nonnull);

char* strsignal(int);

int strcoll(const char* _Nonnull, const char* _Nonnull) __attribute_pure__;
size_t strxfrm(char* __restrict, const char* _Nonnull __restrict, size_t);

#if __ANDROID_API__ >= __ANDROID_API_L__
int strcoll_l(const char* _Nonnull, const char* _Nonnull, locale_t) __attribute_pure__ __INTRODUCED_IN(21);
size_t strxfrm_l(char* __restrict, const char* _Nonnull __restrict, size_t, locale_t) __INTRODUCED_IN(21);
#else
// Implemented as static inlines before 21.
#endif

#if defined(__USE_GNU) && !defined(basename)
/*
 * glibc has a basename in <string.h> that's different to the POSIX one in <libgen.h>.
 * It doesn't modify its argument, and in C++ it's const-correct.
 */
#if defined(__cplusplus)
extern "C++" char* basename(char* _Nonnull) __RENAME(__gnu_basename);
extern "C++" const char* basename(const char* _Nonnull) __RENAME(__gnu_basename);
#else
char* basename(const char* _Nonnull) __RENAME(__gnu_basename) __INTRODUCED_IN(23);
#endif
#endif

void* __memchr_chk(const void* _Nonnull, int, size_t, size_t) __INTRODUCED_IN(23);
void* __memrchr_chk(const void* _Nonnull, int, size_t, size_t) __INTRODUCED_IN(23);
char* __stpncpy_chk2(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t, size_t, size_t)
  __INTRODUCED_IN(21);
char* __strncpy_chk2(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t, size_t, size_t)
  __INTRODUCED_IN(21);
size_t __strlcpy_chk(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t, size_t) __INTRODUCED_IN(17);
size_t __strlcat_chk(char* _Nonnull __restrict, const char* _Nonnull __restrict, size_t, size_t) __INTRODUCED_IN(17);

/* Only used with FORTIFY, but some headers that need it undef FORTIFY, so we
 * have the definition out here.
 */
struct __bionic_zero_size_is_okay_t {};

#if defined(__BIONIC_FORTIFY)
// These can share their implementation between gcc and clang with minimal
// trickery...
#if __ANDROID_API__ >= __ANDROID_API_J_MR1__
__BIONIC_FORTIFY_INLINE
void* memcpy(void* _Nonnull __restrict const dst __pass_object_size0,
        const void* _Nonnull __restrict src, size_t copy_amount) __overloadable {
    return __builtin___memcpy_chk(dst, src, copy_amount, __bos0(dst));
}

__BIONIC_FORTIFY_INLINE
void* memmove(void* const _Nonnull dst __pass_object_size0,
        const void* _Nonnull src, size_t len) __overloadable {
    return __builtin___memmove_chk(dst, src, len, __bos0(dst));
}
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR1__ */

#if __ANDROID_API__ >= __ANDROID_API_L__
__BIONIC_FORTIFY_INLINE
char* stpcpy(char* _Nonnull __restrict const dst __pass_object_size,
        const char* _Nonnull __restrict src) __overloadable {
    return __builtin___stpcpy_chk(dst, src, __bos(dst));
}
#endif /* __ANDROID_API__ >= __ANDROID_API_L__ */

#if __ANDROID_API__ >= __ANDROID_API_J_MR1__
__BIONIC_FORTIFY_INLINE
char* strcpy(char* _Nonnull __restrict const dst __pass_object_size,
        const char* _Nonnull __restrict src) __overloadable {
    return __builtin___strcpy_chk(dst, src, __bos(dst));
}

__BIONIC_FORTIFY_INLINE
char* strcat(char* _Nonnull __restrict const dst __pass_object_size,
        const char* _Nonnull __restrict src) __overloadable {
    return __builtin___strcat_chk(dst, src, __bos(dst));
}

__BIONIC_FORTIFY_INLINE
char* strncat(char* const _Nonnull __restrict dst __pass_object_size,
        const char* _Nonnull __restrict src, size_t n) __overloadable {
    return __builtin___strncat_chk(dst, src, n, __bos(dst));
}

__BIONIC_FORTIFY_INLINE
void* memset(void* const _Nonnull s __pass_object_size0, int c, size_t n)
        __overloadable {
    return __builtin___memset_chk(s, c, n, __bos0(s));
}
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR1__ */


#if defined(__clang__)

#define __error_if_overflows_dst(name, dst, n, what) \
    __enable_if(__bos0(dst) != __BIONIC_FORTIFY_UNKNOWN_SIZE && \
                __bos0(dst) < (n), "selected when the buffer is too small") \
    __errorattr(#name " called with " what " bigger than buffer")

/*
 * N.B. _Nonnull isn't necessary on params, since these functions just emit
 * errors.
 */
__BIONIC_ERROR_FUNCTION_VISIBILITY
void* memcpy(void* dst, const void* src, size_t copy_amount) __overloadable
        __error_if_overflows_dst(memcpy, dst, copy_amount, "size");

__BIONIC_ERROR_FUNCTION_VISIBILITY
void* memmove(void *dst, const void* src, size_t len) __overloadable
        __error_if_overflows_dst(memmove, dst, len, "size");

__BIONIC_ERROR_FUNCTION_VISIBILITY
void* memset(void* s, int c, size_t n) __overloadable
        __error_if_overflows_dst(memset, s, n, "size");

__BIONIC_ERROR_FUNCTION_VISIBILITY
char* stpcpy(char* dst, const char* src) __overloadable
        __error_if_overflows_dst(stpcpy, dst, __builtin_strlen(src), "string");

__BIONIC_ERROR_FUNCTION_VISIBILITY
char* strcpy(char* dst, const char* src) __overloadable
        __error_if_overflows_dst(strcpy, dst, __builtin_strlen(src), "string");

#if __ANDROID_API__ >= __ANDROID_API_M__
__BIONIC_FORTIFY_INLINE
void* memchr(const void* const _Nonnull s __pass_object_size, int c, size_t n)
        __overloadable {
    size_t bos = __bos(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_memchr(s, c, n);
    }

    return __memchr_chk(s, c, n, bos);
}

__BIONIC_FORTIFY_INLINE
void* memrchr(const void* const _Nonnull s __pass_object_size, int c, size_t n)
        __overloadable {
    size_t bos = __bos(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(memrchr)(s, c, n);
    }

    return __memrchr_chk(s, c, n, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_M__ */

#if __ANDROID_API__ >= __ANDROID_API_L__
__BIONIC_FORTIFY_INLINE
char* stpncpy(char* __restrict const _Nonnull dst __pass_object_size,
        const char* __restrict const _Nonnull src __pass_object_size,
        size_t n) __overloadable {
    size_t bos_dst = __bos(dst);
    size_t bos_src = __bos(src);

    /* Ignore dst size checks; they're handled in strncpy_chk */
    if (bos_src == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin___stpncpy_chk(dst, src, n, bos_dst);
    }

    return __stpncpy_chk2(dst, src, n, bos_dst, bos_src);
}

__BIONIC_FORTIFY_INLINE
char* strncpy(char* __restrict const _Nonnull dst __pass_object_size,
        const char* __restrict const _Nonnull src __pass_object_size,
        size_t n) __overloadable {
    size_t bos_dst = __bos(dst);
    size_t bos_src = __bos(src);

    /* Ignore dst size checks; they're handled in strncpy_chk */
    if (bos_src == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin___strncpy_chk(dst, src, n, bos_dst);
    }

    return __strncpy_chk2(dst, src, n, bos_dst, bos_src);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_L__ */

#if __ANDROID_API__ >= __ANDROID_API_J_MR1__
__BIONIC_FORTIFY_INLINE
size_t strlcpy(char* const _Nonnull __restrict dst __pass_object_size,
        const char *_Nonnull __restrict src, size_t size) __overloadable {
    size_t bos = __bos(dst);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(strlcpy)(dst, src, size);
    }

    return __strlcpy_chk(dst, src, size, bos);
}

__BIONIC_FORTIFY_INLINE
size_t strlcat(char* const _Nonnull __restrict dst __pass_object_size,
        const char* _Nonnull __restrict src, size_t size) __overloadable {
    size_t bos = __bos(dst);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(strlcat)(dst, src, size);
    }

    return __strlcat_chk(dst, src, size, bos);
}

/*
 * If we can evaluate the size of s at compile-time, just call __builtin_strlen
 * on it directly. This makes it way easier for compilers to fold things like
 * strlen("Foo") into a constant, as users would expect. -1ULL is chosen simply
 * because it's large.
 */
__BIONIC_FORTIFY_INLINE
size_t strlen(const char* const _Nonnull s __pass_object_size)
        __overloadable __enable_if(__builtin_strlen(s) != -1ULL,
                                   "enabled if s is a known good string.") {
    return __builtin_strlen(s);
}

__BIONIC_FORTIFY_INLINE
size_t strlen(const char* const _Nonnull s __pass_object_size0)
        __overloadable {
    size_t bos = __bos0(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_strlen(s);
    }

    // return __builtin_strlen(s);
    return __strlen_chk(s, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR1__ */

#if  __ANDROID_API__ >= __ANDROID_API_J_MR2__
__BIONIC_FORTIFY_INLINE
char* strchr(const char* const _Nonnull s __pass_object_size0, int c)
        __overloadable {
    size_t bos = __bos0(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_strchr(s, c);
    }

    // return __builtin_strchr(s, c);
    return __strchr_chk(s, c, bos);
}

__BIONIC_FORTIFY_INLINE
char* strrchr(const char* const _Nonnull s __pass_object_size, int c)
        __overloadable {
    size_t bos = __bos(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_strrchr(s, c);
    }

    return __strrchr_chk(s, c, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR2__ */

#if __ANDROID_API__ >= __ANDROID_API_J_MR1__
/* In *many* cases, memset(foo, sizeof(foo), 0) is a mistake where the user has
 * flipped the size + value arguments. However, there may be cases (e.g. with
 * macros) where it's okay for the size to fold to zero. We should warn on this,
 * but we should also provide a FORTIFY'ed escape hatch.
 */
__BIONIC_ERROR_FUNCTION_VISIBILITY
void* memset(void* _Nonnull s, int c, size_t n,
             struct __bionic_zero_size_is_okay_t ok)
        __overloadable
        __error_if_overflows_dst(memset, s, n, "size");

__BIONIC_FORTIFY_INLINE
void* memset(void* const _Nonnull s __pass_object_size0, int c, size_t n,
             struct __bionic_zero_size_is_okay_t ok __attribute__((unused)))
        __overloadable {
    return __builtin___memset_chk(s, c, n, __bos0(s));
}

extern struct __bionic_zero_size_is_okay_t __bionic_zero_size_is_okay;
/* We verify that `c` is non-zero, because as pointless as memset(foo, 0, 0) is,
 * flipping size + count will do nothing.
 */
__BIONIC_ERROR_FUNCTION_VISIBILITY
void* memset(void* _Nonnull s, int c, size_t n) __overloadable
        __enable_if(c && !n, "selected when we'll set zero bytes")
        __RENAME_CLANG(memset)
        __warnattr_real("will set 0 bytes; maybe the arguments got flipped? "
                        "(Add __bionic_zero_size_is_okay as a fourth argument "
                        "to silence this.)");
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR1__ */

#undef __error_zero_size
#undef __error_if_overflows_dst
#else // defined(__clang__)
extern char* __strncpy_real(char* __restrict, const char*, size_t) __RENAME(strncpy);
extern void* __memrchr_real(const void*, int, size_t) __RENAME(memrchr);
extern size_t __strlcpy_real(char* __restrict, const char* __restrict, size_t)
    __RENAME(strlcpy);
extern size_t __strlcat_real(char* __restrict, const char* __restrict, size_t)
    __RENAME(strlcat);

__errordecl(__memchr_buf_size_error, "memchr called with size bigger than buffer");
__errordecl(__memrchr_buf_size_error, "memrchr called with size bigger than buffer");

#if __ANDROID_API__ >= __ANDROID_API_M__
__BIONIC_FORTIFY_INLINE
void* memchr(const void *_Nonnull s __pass_object_size, int c, size_t n) {
    size_t bos = __bos(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_memchr(s, c, n);
    }

    if (__builtin_constant_p(n) && (n > bos)) {
        __memchr_buf_size_error();
    }

    if (__builtin_constant_p(n) && (n <= bos)) {
        return __builtin_memchr(s, c, n);
    }

    return __memchr_chk(s, c, n, bos);
}

__BIONIC_FORTIFY_INLINE
void* memrchr(const void* s, int c, size_t n) {
    size_t bos = __bos(s);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __memrchr_real(s, c, n);
    }

    if (__builtin_constant_p(n) && (n > bos)) {
        __memrchr_buf_size_error();
    }

    if (__builtin_constant_p(n) && (n <= bos)) {
        return __memrchr_real(s, c, n);
    }

    return __memrchr_chk(s, c, n, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_M__ */

#if __ANDROID_API__ >= __ANDROID_API_L__
__BIONIC_FORTIFY_INLINE
char* stpncpy(char* _Nonnull __restrict dst, const char* _Nonnull __restrict src, size_t n) {
    size_t bos_dst = __bos(dst);
    size_t bos_src = __bos(src);

    if (bos_src == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin___stpncpy_chk(dst, src, n, bos_dst);
    }

    if (__builtin_constant_p(n) && (n <= bos_src)) {
        return __builtin___stpncpy_chk(dst, src, n, bos_dst);
    }

    size_t slen = __builtin_strlen(src);
    if (__builtin_constant_p(slen)) {
        return __builtin___stpncpy_chk(dst, src, n, bos_dst);
    }

    return __stpncpy_chk2(dst, src, n, bos_dst, bos_src);
}

__BIONIC_FORTIFY_INLINE
char* strncpy(char* _Nonnull __restrict dst, const char* _Nonnull __restrict src, size_t n) {
    size_t bos_dst = __bos(dst);
    size_t bos_src = __bos(src);

    if (bos_src == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __strncpy_real(dst, src, n);
    }

    if (__builtin_constant_p(n) && (n <= bos_src)) {
        return __builtin___strncpy_chk(dst, src, n, bos_dst);
    }

    size_t slen = __builtin_strlen(src);
    if (__builtin_constant_p(slen)) {
        return __builtin___strncpy_chk(dst, src, n, bos_dst);
    }

    return __strncpy_chk2(dst, src, n, bos_dst, bos_src);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_L__ */

#if __ANDROID_API__ >= __ANDROID_API_J_MR1__
__BIONIC_FORTIFY_INLINE
size_t strlcpy(char* _Nonnull __restrict dst __pass_object_size,
        const char* _Nonnull __restrict src, size_t size) {
    size_t bos = __bos(dst);

    // Compiler doesn't know destination size. Don't call __strlcpy_chk
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __strlcpy_real(dst, src, size);
    }

    // Compiler can prove, at compile time, that the passed in size
    // is always <= the actual object size. Don't call __strlcpy_chk
    if (__builtin_constant_p(size) && (size <= bos)) {
        return __strlcpy_real(dst, src, size);
    }

    return __strlcpy_chk(dst, src, size, bos);
}

__BIONIC_FORTIFY_INLINE
size_t strlcat(char* _Nonnull __restrict dst, const char* _Nonnull __restrict src, size_t size) {
    size_t bos = __bos(dst);

    // Compiler doesn't know destination size. Don't call __strlcat_chk
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __strlcat_real(dst, src, size);
    }

    // Compiler can prove, at compile time, that the passed in size
    // is always <= the actual object size. Don't call __strlcat_chk
    if (__builtin_constant_p(size) && (size <= bos)) {
        return __strlcat_real(dst, src, size);
    }

    return __strlcat_chk(dst, src, size, bos);
}

__BIONIC_FORTIFY_INLINE
size_t strlen(const char* _Nonnull s) __overloadable {
    size_t bos = __bos(s);

    // Compiler doesn't know destination size. Don't call __strlen_chk
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_strlen(s);
    }

    size_t slen = __builtin_strlen(s);
    if (__builtin_constant_p(slen)) {
        return slen;
    }

    return __strlen_chk(s, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR1__ */

#if  __ANDROID_API__ >= __ANDROID_API_J_MR2__
__BIONIC_FORTIFY_INLINE
char* strchr(const char* _Nonnull s, int c) {
    size_t bos = __bos(s);

    // Compiler doesn't know destination size. Don't call __strchr_chk
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_strchr(s, c);
    }

    size_t slen = __builtin_strlen(s);
    if (__builtin_constant_p(slen) && (slen < bos)) {
        return __builtin_strchr(s, c);
    }

    return __strchr_chk(s, c, bos);
}

__BIONIC_FORTIFY_INLINE
char* strrchr(const char* _Nonnull s, int c) {
    size_t bos = __bos(s);

    // Compiler doesn't know destination size. Don't call __strrchr_chk
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __builtin_strrchr(s, c);
    }

    size_t slen = __builtin_strlen(s);
    if (__builtin_constant_p(slen) && (slen < bos)) {
        return __builtin_strrchr(s, c);
    }

    return __strrchr_chk(s, c, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_J_MR2__ */
#endif /* defined(__clang__) */
#endif /* defined(__BIONIC_FORTIFY) */

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

__END_DECLS

#endif /* _STRING_H */
