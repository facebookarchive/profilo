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

#ifndef _TIME_H_
#define _TIME_H_

#include <sys/cdefs.h>
#include <sys/time.h>
#include <xlocale.h>

__BEGIN_DECLS

#define CLOCKS_PER_SEC 1000000

extern char* tzname[];
extern int daylight;
extern long int timezone;

struct sigevent;

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
  long int tm_gmtoff;
  const char* tm_zone;
};

#define TM_ZONE tm_zone

time_t time(time_t*);
int nanosleep(const struct timespec*, struct timespec*);

char* asctime(const struct tm*);
char* asctime_r(const struct tm*, char*);

double difftime(time_t, time_t);
time_t mktime(struct tm*);

struct tm* localtime(const time_t*);
struct tm* localtime_r(const time_t*, struct tm*);

struct tm* gmtime(const time_t*);
struct tm* gmtime_r(const time_t*, struct tm*);

char* strptime(const char*, const char*, struct tm*);
size_t strftime(char*, size_t, const char*, const struct tm*);

#if __ANDROID_API__ >= __ANDROID_API_L__
size_t strftime_l(char*, size_t, const char*, const struct tm*, locale_t) __INTRODUCED_IN(21);
#else
// Implemented as static inline before 21.
#endif

char* ctime(const time_t*);
char* ctime_r(const time_t*, char*);

void tzset(void);

clock_t clock(void);

int clock_getcpuclockid(pid_t, clockid_t*) __INTRODUCED_IN(23);

int clock_getres(clockid_t, struct timespec*);
int clock_gettime(clockid_t, struct timespec*);
int clock_nanosleep(clockid_t, int, const struct timespec*, struct timespec*);
int clock_settime(clockid_t, const struct timespec*);

int timer_create(int, struct sigevent*, timer_t*);
int timer_delete(timer_t);
int timer_settime(timer_t, int, const struct itimerspec*, struct itimerspec*);
int timer_gettime(timer_t, struct itimerspec*);
int timer_getoverrun(timer_t);

/* Non-standard extensions that are in the BSDs and glibc. */
time_t timelocal(struct tm*) __INTRODUCED_IN(12);
time_t timegm(struct tm*) __INTRODUCED_IN(12);

__END_DECLS

#endif /* _TIME_H_ */
