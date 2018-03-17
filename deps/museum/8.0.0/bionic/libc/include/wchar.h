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
#ifndef _WCHAR_H_
#define _WCHAR_H_

#include <sys/cdefs.h>
#include <stdio.h>

#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include <xlocale.h>

#include <bits/mbstate_t.h>
#include <bits/wchar_limits.h>
#include <bits/wctype.h>

__BEGIN_DECLS

enum {
    WC_TYPE_INVALID = 0,
    WC_TYPE_ALNUM,
    WC_TYPE_ALPHA,
    WC_TYPE_BLANK,
    WC_TYPE_CNTRL,
    WC_TYPE_DIGIT,
    WC_TYPE_GRAPH,
    WC_TYPE_LOWER,
    WC_TYPE_PRINT,
    WC_TYPE_PUNCT,
    WC_TYPE_SPACE,
    WC_TYPE_UPPER,
    WC_TYPE_XDIGIT,
    WC_TYPE_MAX
};

wint_t            btowc(int);
int               fwprintf(FILE *, const wchar_t *, ...);
int               fwscanf(FILE *, const wchar_t *, ...);
wint_t            fgetwc(FILE *);
wchar_t          *fgetws(wchar_t *, int, FILE *);
wint_t            fputwc(wchar_t, FILE *);
int               fputws(const wchar_t *, FILE *);
int               fwide(FILE *, int);
wint_t            getwc(FILE *);
wint_t            getwchar(void);
int               mbsinit(const mbstate_t *);
size_t            mbrlen(const char *, size_t, mbstate_t *);
size_t            mbrtowc(wchar_t *, const char *, size_t, mbstate_t *);
size_t mbsrtowcs(wchar_t*, const char**, size_t, mbstate_t*);
size_t mbsnrtowcs(wchar_t*, const char**, size_t, size_t, mbstate_t*) __INTRODUCED_IN(21);
wint_t            putwc(wchar_t, FILE *);
wint_t            putwchar(wchar_t);
int               swprintf(wchar_t *, size_t, const wchar_t *, ...);
int               swscanf(const wchar_t *, const wchar_t *, ...);
wint_t            ungetwc(wint_t, FILE *);
int vfwprintf(FILE*, const wchar_t*, va_list);
int vfwscanf(FILE*, const wchar_t*, va_list) __INTRODUCED_IN(21);
int vswprintf(wchar_t*, size_t, const wchar_t*, va_list);
int vswscanf(const wchar_t*, const wchar_t*, va_list) __INTRODUCED_IN(21);
int vwprintf(const wchar_t*, va_list);
int vwscanf(const wchar_t*, va_list) __INTRODUCED_IN(21);
wchar_t* wcpcpy (wchar_t*, const wchar_t *);
wchar_t* wcpncpy (wchar_t*, const wchar_t *, size_t);
size_t            wcrtomb(char *, wchar_t, mbstate_t *);
int               wcscasecmp(const wchar_t *, const wchar_t *);
int wcscasecmp_l(const wchar_t*, const wchar_t*, locale_t) __INTRODUCED_IN(23);
wchar_t          *wcscat(wchar_t *, const wchar_t *);
wchar_t          *wcschr(const wchar_t *, wchar_t);
int               wcscmp(const wchar_t *, const wchar_t *);
int               wcscoll(const wchar_t *, const wchar_t *);
wchar_t          *wcscpy(wchar_t *, const wchar_t *);
size_t            wcscspn(const wchar_t *, const wchar_t *);
size_t            wcsftime(wchar_t *, size_t, const wchar_t *, const struct tm *);
size_t            wcslen(const wchar_t *);
int               wcsncasecmp(const wchar_t *, const wchar_t *, size_t);
int wcsncasecmp_l(const wchar_t*, const wchar_t*, size_t, locale_t) __INTRODUCED_IN(23);
wchar_t          *wcsncat(wchar_t *, const wchar_t *, size_t);
int               wcsncmp(const wchar_t *, const wchar_t *, size_t);
wchar_t          *wcsncpy(wchar_t *, const wchar_t *, size_t);
size_t wcsnrtombs(char*, const wchar_t**, size_t, size_t, mbstate_t*) __INTRODUCED_IN(21);
wchar_t          *wcspbrk(const wchar_t *, const wchar_t *);
wchar_t          *wcsrchr(const wchar_t *, wchar_t);
size_t wcsrtombs(char*, const wchar_t**, size_t, mbstate_t*);
size_t            wcsspn(const wchar_t *, const wchar_t *);
wchar_t          *wcsstr(const wchar_t *, const wchar_t *);
double wcstod(const wchar_t*, wchar_t**);
float wcstof(const wchar_t*, wchar_t**) __INTRODUCED_IN(21);
wchar_t* wcstok(wchar_t*, const wchar_t*, wchar_t**);
long wcstol(const wchar_t*, wchar_t**, int);
long long wcstoll(const wchar_t*, wchar_t**, int) __INTRODUCED_IN(21);
long double wcstold(const wchar_t*, wchar_t**) __INTRODUCED_IN(21);
unsigned long wcstoul(const wchar_t*, wchar_t**, int);
unsigned long long wcstoull(const wchar_t*, wchar_t**, int) __INTRODUCED_IN(21);
int               wcswidth(const wchar_t *, size_t);
size_t            wcsxfrm(wchar_t *, const wchar_t *, size_t);
int               wctob(wint_t);
int               wcwidth(wchar_t);
wchar_t          *wmemchr(const wchar_t *, wchar_t, size_t);
int               wmemcmp(const wchar_t *, const wchar_t *, size_t);
wchar_t          *wmemcpy(wchar_t *, const wchar_t *, size_t);
#if defined(__USE_GNU)
wchar_t* wmempcpy(wchar_t*, const wchar_t*, size_t) __INTRODUCED_IN(23);
#endif
wchar_t          *wmemmove(wchar_t *, const wchar_t *, size_t);
wchar_t          *wmemset(wchar_t *, wchar_t, size_t);
int               wprintf(const wchar_t *, ...);
int               wscanf(const wchar_t *, ...);

#if __ANDROID_API__ >= __ANDROID_API_L__
long long wcstoll_l(const wchar_t*, wchar_t**, int, locale_t) __INTRODUCED_IN(21);
unsigned long long wcstoull_l(const wchar_t*, wchar_t**, int, locale_t) __INTRODUCED_IN(21);
long double wcstold_l(const wchar_t*, wchar_t**, locale_t) __INTRODUCED_IN(21);

int wcscoll_l(const wchar_t* _Nonnull, const wchar_t* _Nonnull, locale_t) __attribute_pure__
    __INTRODUCED_IN(21);
size_t wcsxfrm_l(wchar_t*, const wchar_t* _Nonnull, size_t, locale_t) __INTRODUCED_IN(21);
#else
// Implemented as static inlines before 21.
#endif

size_t wcslcat(wchar_t*, const wchar_t*, size_t);
size_t wcslcpy(wchar_t*, const wchar_t*, size_t);

FILE* open_wmemstream(wchar_t**, size_t*) __INTRODUCED_IN(23);
wchar_t* wcsdup(const wchar_t*);
size_t wcsnlen(const wchar_t*, size_t);

__END_DECLS

#endif /* _WCHAR_H_ */
