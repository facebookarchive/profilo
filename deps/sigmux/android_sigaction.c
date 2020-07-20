/*
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

#include "android_sigaction.h"

#ifdef ANDROID

#include "sigmux.h"

#include <android/log.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/system_properties.h>

/**
 * For pointers to sigaction64 in libc for Android OS >= 9
 */
typedef int (*sigaction64_fn)(
    int signum,
    const struct sigaction64* act,
    struct sigaction64* oldact);

typedef int (*sigemptyset_fn)(sigset_t* set);
typedef int (*sigaddset_fn)(sigset_t* set, int signum);
typedef int (*sigismember_fn)(const sigset_t* set, int signum);

#define TAG "sigmux_sigaction_wrapper"
#define LOGI(x) __android_log_write(ANDROID_LOG_INFO, TAG, x)
#define LOGW(x) __android_log_write(ANDROID_LOG_WARN, TAG, x)

static struct {
  sigaction64_fn real_sigaction64;
  sigemptyset_fn sigemptyset64;
  sigaddset_fn sigaddset64;
  sigismember_fn sigismember64;
  sigmux_sigaction_function real_sigaction;
  bool has_sigaction64;
} sigaction_table = {
    .has_sigaction64 = false,
};

void copy_to_sigaction64(struct sigaction* act, struct sigaction64* act64) {
  if (!act || !act64) {
    return;
  }
  act64->sa_sigaction = act->sa_sigaction;
  act64->sa_flags = act->sa_flags;
  act64->sa_restorer = act->sa_restorer;
  act64->sa_handler = act->sa_handler;
  sigaction_table.sigemptyset64(&act64->sa_mask);
  for (int i = 1; i < NSIG; i++) {
    if (sigismember(&act->sa_mask, i)) {
      sigaction_table.sigaddset64(&act64->sa_mask, i);
    }
  }
}

void copy_from_sigaction64(struct sigaction64* act64, struct sigaction* act) {
  if (!act || !act64) {
    return;
  }
  act->sa_sigaction = act64->sa_sigaction;
  act->sa_flags = act64->sa_flags;
  act->sa_restorer = act64->sa_restorer;
  act->sa_handler = act64->sa_handler;
  sigemptyset(&act->sa_mask);
  for (int i = 1; i < NSIG; i++) {
    if (sigaction_table.sigismember64(&act64->sa_mask, i)) {
      sigaddset(&act->sa_mask, i);
    }
  }
}

int sigaction_internal_wrapper(int signum, struct sigaction* act, struct sigaction* oldact) {
  if (sigaction_table.has_sigaction64) {
    struct sigaction64 act64 = { .sa_sigaction = NULL };
    struct sigaction64 oldact64 = { .sa_sigaction = NULL };
    struct sigaction64 *act64ptr, *oldact64ptr;
    act64ptr = act ? &act64 : NULL;
    oldact64ptr = oldact ? &oldact64 : NULL;
    copy_to_sigaction64(act, act64ptr);
    int res = sigaction_table.real_sigaction64(signum, act64ptr, oldact64ptr);
    copy_from_sigaction64(oldact64ptr, oldact);
    return res;
  } else {
    return sigaction_table.real_sigaction(signum, act, oldact);
  }
}

bool init() {
  void* libc = dlopen("libc.so", RTLD_LOCAL);

  if (!libc) {
    __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open libc due to error: %s\n", dlerror());
    return false;
  }
  sigaction_table.real_sigaction64 = (sigaction64_fn)dlsym(libc, "sigaction64");
  sigaction_table.sigemptyset64 = (sigemptyset_fn)dlsym(libc, "sigemptyset64");
  sigaction_table.sigaddset64 = (sigaddset_fn)dlsym(libc, "sigaddset64");
  sigaction_table.sigismember64 = (sigismember_fn)dlsym(libc, "sigismember64");
  sigaction_table.real_sigaction = (sigmux_sigaction_function)dlsym(libc, "sigaction");
  dlclose(libc);

  if (sigaction_table.real_sigaction64 &&
      sigaction_table.sigemptyset64 &&
      sigaction_table.sigaddset64 &&
      sigaction_table.sigismember64) {
    sigaction_table.has_sigaction64 = true;
    LOGI("init(): libc sigaction64 installed");
    return true;
  } else if (sigaction_table.real_sigaction) {
    LOGI("init(): libc sigaction installed");
    return true;
  } else {
    // No valid set of libc.so symbols were located
    LOGW("init(): failed, no libc sigaction function installed");
    return false;
  }
}

void install_sigaction_wrapper() {
  if (init()) {
    sigmux_set_real_sigaction(&sigaction_internal_wrapper);
  }
}

#endif

