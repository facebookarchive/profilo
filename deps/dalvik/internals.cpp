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


#include <dalvik-subset/internals.h>
#include <dlfcn.h>
#include <fb/Build.h>
#include <force_dlopen.h>
#include <mistake.h>

using namespace facebook::mistake;
using namespace facebook::build;

static void*
open_libc_1()
{
  void* libc = Build::getAndroidSdk() >= 24
               ? force_dlopen("libc.so", RTLD_LOCAL)
               : dlopen("libc.so", RTLD_LOCAL);
  if (libc == NULL) {
    throw_runtime("dlopen(\"%s\"): %s", "libc.so", dlerror());
  }
  return libc;
}

void*
open_libc()
{
  static void* libc = open_libc_1();
  return libc;
}

static void*
open_libdvm_1()
{
  // don't need to check for force_dlopen here since api24 doesn't have libdvm
  void* libdvm = dlopen("libdvm.so", RTLD_LOCAL);
  if (libdvm == NULL) {
    throw_runtime("dlopen(\"%s\"): %s", "libdvm.so", dlerror());
  }
  return libdvm;
}

 __attribute__ ((visibility ("default")))
void*
open_libdvm()
{
  static void* libdvm = open_libdvm_1();
  return libdvm;
}

static void*
open_libart_1()
{
  void* libart = Build::getAndroidSdk() >= 24
               ? force_dlopen("libart.so", RTLD_LOCAL)
               : dlopen("libart.so", RTLD_LOCAL);
  if (libart == NULL) {
    throw_runtime("dlopen(\"%s\"): %s", "libart.so", dlerror());
  }
  return libart;
}

void*
open_libart()
{
  static void* libart = open_libart_1();
  return libart;
}

/**
 * Finds all symbols in the given shared library, assigning the value
 * of each to the word pointed to by value in each struct.
 */
__attribute__ ((visibility ("default")))
void
ensure_symbols(
  void* lib,
  const struct SymbolSpec* ss,
  unsigned nr_ss)
{
  for (unsigned i = 0; i < nr_ss; ++i) {
    void** fnptr = (void**) ss[i].value;
    if (*fnptr == NULL) {
      *fnptr = dlsym(lib, ss[i].name[0]);
      if (*fnptr == NULL && ss[i].name[1] != NULL) {
        *fnptr = dlsym(lib, ss[i].name[1]);
      }

      if (*fnptr == NULL && !ss[i].optional) {
        throw_runtime("could not find %s", ss[i].name[0]);
      }
    }
  }
}
