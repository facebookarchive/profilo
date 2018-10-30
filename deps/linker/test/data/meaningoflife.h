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

#include <time.h>

extern int meaning_of_life;

struct large {
  double a;
  int b;
  double c;
  char const* d;
  char e, f, g;
};

clock_t call_clock();

double nice1(int one);
int nice2(int one, double two);

#define EVIL2_MUNGE_CALLBACK_INT(initial)     (-(initial))
#define EVIL3_MUNGE_CALLBACK_INT(a, b, c)     (((a) + (b)) * (c))
void evil1(struct large one, int two, void (*cb)(struct large*, int, void*), void* unk);
void* evil2(int one, struct large two, void (*cb)(struct large*, int, void*), void* unk);
struct large evil3(int one, int two, int three, struct large four, void (*cb)(struct large*, int, void*), void* unk);
