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

#pragma once

#include <linker/log_assert.h>

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

class ReaderLock {
private:
  pthread_rwlock_t* const mutex_;

public:
  inline ReaderLock(pthread_rwlock_t* mutex)
      : mutex_(mutex) {
    auto ret = pthread_rwlock_rdlock(mutex_);
    if (ret != 0) {
      log_assert("pthread_rwlock_rdlock returned %s", strerror(ret));
    }
  }

  inline ~ReaderLock() {
    auto ret = pthread_rwlock_unlock(mutex_);
    if (ret != 0) {
      log_assert("pthread_rwlock_unlock returned %s", strerror(ret));
    }
  }
};

class WriterLock {
private:
  pthread_rwlock_t *mutex_;

public:
  inline WriterLock(pthread_rwlock_t *mutex)
      : mutex_(mutex) {
    int ret = pthread_rwlock_wrlock(mutex_);
    if (ret != 0) {
      log_assert("pthread_rwlock_wrlock returned %s", strerror(ret));
    }
  }

  inline ~WriterLock() {
    int ret = pthread_rwlock_unlock(mutex_);
    if (ret != 0) {
      log_assert("pthread_rwlock_unlock returned %s", strerror(ret));
    }
  }
};
