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

#include <linkertests/test.h>

#include <dlfcn.h>
#include <link.h>
#include <gtest/gtest.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdexcept>

#include <fb/Build.h>
#include <cppdistract/dso.h>

#include <linker/sharedlibs.h>
#include <linkertestdata/var.h>

using namespace facebook::linker;

struct ElfSharedLibDataTest : public BaseTest {
  ElfSharedLibDataTest()
    : libtarget(LIBDIR("libtarget.so")) { }

  virtual void SetUp() {
    BaseTest::SetUp();

    lib = sharedLib("libtarget.so");
    ASSERT_TRUE(lib);
    ASSERT_FALSE(lib.usesGnuHashTable());
  }

  facebook::cppdistract::dso const libtarget;
  elfSharedLibData lib;
};

TEST_F(ElfSharedLibDataTest, testElfLookupDefined) {
  auto call_clock = libtarget.get_symbol<int()>("call_clock");
  ASSERT_NE(nullptr, call_clock);

  auto sym = lib.find_symbol_by_name("call_clock");
  ASSERT_NE(nullptr, sym);
  ASSERT_EQ(call_clock, lib.getLoadedAddress(sym));
}

TEST_F(ElfSharedLibDataTest, testElfLookupUndefined) {
  auto sym = lib.find_symbol_by_name("clock");
  ASSERT_NE(nullptr, sym);
  ASSERT_EQ(0, sym->st_value);
}

TEST_F(ElfSharedLibDataTest, testElfLookupBad) {
  auto sym = lib.find_symbol_by_name("supercalifragilisticexpialidocious");
  ASSERT_EQ(nullptr, sym);
}

TEST_F(ElfSharedLibDataTest, testGetPltRelocations) {
  auto sym = lib.find_symbol_by_name("clock");
  ASSERT_NE(nullptr, sym);

  auto pltrelocs = lib.get_plt_relocations(sym);
  ASSERT_EQ(1, pltrelocs.size());
  ASSERT_EQ(&clock, *pltrelocs[0]);
}

TEST_F(ElfSharedLibDataTest, testGetDynamicSymbolRelocation) {
  auto symrelocs = lib.get_relocations(&var);
  ASSERT_EQ(1, symrelocs.size());
  ASSERT_EQ(&var, *symrelocs[0]);
}

// TEST_F(ElfSharedLibDataTest, testAliasedPltRelocations) {
//   // TODO: is it even possible to generate a .so that has multiple PLT entries for the same symbol?
// }

struct ElfSharedLibDataTestGnuHash : public BaseTest {
  ElfSharedLibDataTestGnuHash()
    : libgnu(LIBDIR("libgnu.so")) { }

  virtual void SetUp() {
    BaseTest::SetUp();

    lib = sharedLib("libgnu.so");
    ASSERT_TRUE(lib);
    ASSERT_TRUE(lib.usesGnuHashTable());
  }

  facebook::cppdistract::dso const libgnu;
  elfSharedLibData lib;
};

TEST_F(ElfSharedLibDataTestGnuHash, testGnuLookupDefined) {
  auto call_clock = libgnu.get_symbol<int()>("call_clock");
  ASSERT_NE(nullptr, call_clock);

  auto sym = lib.find_symbol_by_name("call_clock");
  ASSERT_NE(nullptr, sym);
  ASSERT_EQ(call_clock, lib.getLoadedAddress(sym));
}

TEST_F(ElfSharedLibDataTestGnuHash, testGnuLookupUndefined) {
  auto sym = lib.find_symbol_by_name("clock");
  ASSERT_NE(nullptr, sym);
  ASSERT_EQ(0, sym->st_value);
}

TEST_F(ElfSharedLibDataTestGnuHash, testGnuLookupBad) {
  auto sym = lib.find_symbol_by_name("supercalifragilisticexpialidocious");
  ASSERT_EQ(nullptr, sym);
}

TEST_F(ElfSharedLibDataTestGnuHash, testGetPltRelocations) {
  auto sym = lib.find_symbol_by_name("clock");
  ASSERT_NE(nullptr, sym);

  auto pltrelocs = lib.get_plt_relocations(sym);
  ASSERT_EQ(1, pltrelocs.size());
  ASSERT_EQ(&clock, *pltrelocs[0]);
}

TEST_F(ElfSharedLibDataTestGnuHash, testGetDynamicSymbolRelocation) {
  auto symrelocs = lib.get_relocations(&var);
  ASSERT_EQ(1, symrelocs.size());
  ASSERT_EQ(&var, *symrelocs[0]);
}
