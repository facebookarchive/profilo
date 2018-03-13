/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_ENTRYPOINTS_MATH_ENTRYPOINTS_H_
#define ART_RUNTIME_ENTRYPOINTS_MATH_ENTRYPOINTS_H_

#include <stdint.h>

extern "C" double art_l2d(int64_t l);
extern "C" float art_l2f(int64_t l);
extern "C" int64_t art_d2l(double d);
extern "C" int32_t art_d2i(double d);
extern "C" int64_t art_f2l(float f);
extern "C" int32_t art_f2i(float f);

#endif  // ART_RUNTIME_ENTRYPOINTS_MATH_ENTRYPOINTS_H_
