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

#include <plthooktestdata/meaningoflife.h>

int meaning_of_life = 42;

double nice1(int one) {
  return one * -42.0;
}

int nice2(int one, double two) {
  return one * two;
}

void evil1(struct large one, int two, void (*cb)(struct large*, int, void*), void* unk) {
  cb(&one, two, unk);
}

void* evil2(int one, struct large two, void (*cb)(struct large*, int, void*), void* unk) {
  cb(&two, EVIL2_MUNGE_CALLBACK_INT(one), unk);
  return unk;
}

struct large evil3(int one, int two, int three, struct large four, void (*cb)(struct large*, int, void*), void* unk) {
  cb(&four, EVIL3_MUNGE_CALLBACK_INT(one, two, three), unk);
  return four;
}
