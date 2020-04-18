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

#include <stdbool.h>
#include <time.h>
#include <unwind.h>
#include <plthooktestdata/meaningoflife.h>
#include <plthooktestdata/target.h>

clock_t call_clock() {
  return clock();
}

int ask() {
  return meaning_of_life;
}

int add_foo_and_bar() {
  return foo() + bar();
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

_Unwind_Reason_Code unwind_callback(struct _Unwind_Context* context, void* arg) {
  unsigned* num_frames = arg;
  ++*num_frames;
  // For whatever reason, on 32-bit ARM, both libgcc and LLVM's unwinders get
  // stuck in an infinite loop inside art_quick_generic_jni_trampoline in
  // libart.so. Just cut off the stack trace at a reasonable point to prevent
  // this. The infinite loop only occurs on 32-bit ARM, but it's simplest to
  // make all platforms behave the same.
  return *num_frames >= MAX_BACKTRACE_FRAMES ? _URC_END_OF_STACK
                                             : _URC_NO_REASON;
}

bool call_unwind_backtrace(unsigned* num_frames) {
  *num_frames = 0;
  _Unwind_Reason_Code rc = _Unwind_Backtrace(unwind_callback, num_frames);
#if defined(__arm__)
  // libgcc's unwinder returns _URC_FAILURE (on 32-bit ARM) or
  // _URC_FATAL_PHASE1_ERROR (on other platforms)  if the callback returns
  // anything other than _URC_NO_REASON, so just assume that's okay. LLVM's
  // unwinder doesn't have this problem, but checking for that here is more
  // trouble than it's worth IMO. Unfortunately _URC_FAILURE is only defined for
  // 32-bit ARM, so we still need some preprocessor conditionals.
  return rc == _URC_END_OF_STACK || rc == _URC_FAILURE;
#else
  return rc == _URC_END_OF_STACK || rc == _URC_FATAL_PHASE1_ERROR;
#endif
}
