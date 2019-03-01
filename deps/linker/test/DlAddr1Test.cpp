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

#include <link.h>
#include <gtest/gtest.h>
#include <folly/ScopeGuard.h>

#include <linker/link.h>

int meaning_of_life() {
  return 42;
}

static bool null_dladdr_info_ = false;
int dladdr(const void* addr, Dl_info *info) {
  static auto real_dladdr =
    reinterpret_cast<int(*)(const void*, Dl_info*)>(dlsym(RTLD_NEXT, "dladdr"));

  int ret = real_dladdr(addr, info);
  if (null_dladdr_info_) {
    info->dli_sname = nullptr;
    info->dli_saddr = nullptr;
  }
  return ret;
}

struct DlAddr1Test : public BaseTest { };

TEST_F(DlAddr1Test, testDladdr1) {
  ElfW(Sym) const* sym;
  Dl_info info;
  ASSERT_EQ(1, dladdr1((void*)&meaning_of_life, &info, (void**)&sym, RTLD_DL_SYMENT));
  ASSERT_EQ(&meaning_of_life, info.dli_saddr);
  ASSERT_NE(nullptr, sym);
  ASSERT_LT(0U, sym->st_size);
}

TEST_F(DlAddr1Test, testDladdr1NullTolerance) {
  null_dladdr_info_ = true;
  SCOPE_EXIT {
    null_dladdr_info_ = false;
  };

  ElfW(Sym) const* sym;
  Dl_info info;
  ASSERT_EQ(0, dladdr1((void*)&meaning_of_life, &info, (void**)&sym, RTLD_DL_SYMENT));
}
