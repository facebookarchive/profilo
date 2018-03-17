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

// https://raw.githubusercontent.com/android/platform_bionic/android-7.0.0_r33/libc/private/bionic_tls.h
// This file is completely stripped, we only need the value of TLS_SLOT_ART_THREAD_SELF
#ifndef __BIONIC_PRIVATE_BIONIC_TLS_H_
#define __BIONIC_PRIVATE_BIONIC_TLS_H_
#define _BIONIC_PRIVATE_BIONIC_TLS_H_
#define _BIONIC_PRIVATE_BIONIC_TLS_H

#define TLS_SLOT_ART_THREAD_SELF 7

#endif /* __BIONIC_PRIVATE_BIONIC_TLS_H_ */
