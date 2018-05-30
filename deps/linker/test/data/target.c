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

#include <time.h>
#include <plthooktestdata/meaningoflife.h>

clock_t call_clock() {
  return clock();
}

int ask() {
  return meaning_of_life;
}

double call_nice1(int one) {
  return nice1(one);
}

int call_nice2(int one, double two) {
  return nice2(one, two);
}

void call_evil1(struct large one, int two, void (*cb)(struct large*, int, void*), void* unk) {
  evil1(one, two, cb, unk);
}

void* call_evil2(int one, struct large two, void (*cb)(struct large*, int, void*), void* unk) {
  return evil2(one, two, cb, unk);
}

struct large call_evil3(int one, int two, int three, struct large four, void (*cb)(struct large*, int, void*), void* unk) {
  return evil3(one, two, three, four, cb, unk);
}
