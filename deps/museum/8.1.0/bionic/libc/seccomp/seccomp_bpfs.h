/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SECCOMP_BPFS_H
#define SECCOMP_BPFS_H

#include <stddef.h>
#include <linux/seccomp.h>

extern const struct sock_filter arm_filter[];
extern const size_t arm_filter_size;
extern const struct sock_filter arm64_filter[];
extern const size_t arm64_filter_size;
extern const struct sock_filter x86_filter[];
extern const size_t x86_filter_size;
extern const struct sock_filter x86_64_filter[];
extern const size_t x86_64_filter_size;
extern const struct sock_filter mips_filter[];
extern const size_t mips_filter_size;
extern const struct sock_filter mips64_filter[];
extern const size_t mips64_filter_size;

#endif
