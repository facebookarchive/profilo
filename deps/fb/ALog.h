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

/** @file ALog.h
 *
 *  Very simple android only logging. Define LOG_TAG to enable the macros.
 */

#pragma once

#ifdef __ANDROID__

#include <android/log.h>

namespace facebook {
namespace alog {

template <typename... ARGS>
inline void
log(int level, const char* tag, const char* msg, ARGS... args) noexcept {
  __android_log_print(level, tag, msg, args...);
}

template <typename... ARGS>
inline void log(int level, const char* tag, const char* msg) noexcept {
  __android_log_write(level, tag, msg);
}

template <typename... ARGS>
inline void logv(const char* tag, const char* msg, ARGS... args) noexcept {
  log(ANDROID_LOG_VERBOSE, tag, msg, args...);
}

template <typename... ARGS>
inline void logd(const char* tag, const char* msg, ARGS... args) noexcept {
  log(ANDROID_LOG_DEBUG, tag, msg, args...);
}

template <typename... ARGS>
inline void logi(const char* tag, const char* msg, ARGS... args) noexcept {
  log(ANDROID_LOG_INFO, tag, msg, args...);
}

template <typename... ARGS>
inline void logw(const char* tag, const char* msg, ARGS... args) noexcept {
  log(ANDROID_LOG_WARN, tag, msg, args...);
}

template <typename... ARGS>
inline void loge(const char* tag, const char* msg, ARGS... args) noexcept {
  log(ANDROID_LOG_ERROR, tag, msg, args...);
}

template <typename... ARGS>
inline void logf(const char* tag, const char* msg, ARGS... args) noexcept {
  log(ANDROID_LOG_FATAL, tag, msg, args...);
}

#ifdef LOG_TAG
#define ALOGV(...) ::facebook::alog::logv(LOG_TAG, __VA_ARGS__)
#define ALOGD(...) ::facebook::alog::logd(LOG_TAG, __VA_ARGS__)
#define ALOGI(...) ::facebook::alog::logi(LOG_TAG, __VA_ARGS__)
#define ALOGW(...) ::facebook::alog::logw(LOG_TAG, __VA_ARGS__)
#define ALOGE(...) ::facebook::alog::loge(LOG_TAG, __VA_ARGS__)
#define ALOGF(...) ::facebook::alog::logf(LOG_TAG, __VA_ARGS__)
#endif

} // namespace alog
} // namespace facebook

#else
#define ALOGV(...) ((void)0)
#define ALOGD(...) ((void)0)
#define ALOGI(...) ((void)0)
#define ALOGW(...) ((void)0)
#define ALOGE(...) ((void)0)
#define ALOGF(...) ((void)0)
#endif
