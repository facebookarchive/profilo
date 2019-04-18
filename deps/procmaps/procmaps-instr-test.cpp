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

#include <gtest/gtest.h>
#include <procmaps.h>
#include <unistd.h>

#include <folly/ScopeGuard.h>

extern "C" {
  extern char const* procmaps_test_snapshot;
}

constexpr char const* test_data =
    "00400000-004ef000 r-xp 00000000 fc:01 12616207                           /bin/bash\n"
    "006ef000-006f0000 r--p 000ef000 fc:01 12616207                           /bin/bash\n"
    "006f0000-006f9000 rw-p 000f0000 fc:01 12616207                           /bin/bash\n"
    "006f9000-006ff000 rw-p 00000000 00:00 0 \n"
    "017a9000-01825000 rw-p 00000000 00:00 0                                  [heap]\n"
    "7fbca87c9000-7fbca87d4000 r-xp 00000000 fc:01 6293131                    /lib/x86_64-linux-gnu/libnss_files-2.19.so\n"
    "7fbca87d4000-7fbca89d3000 ---p 0000b000 fc:01 6293131                    /lib/x86_64-linux-gnu/libnss_files-2.19.so\n"
    "7fbca89d3000-7fbca89d4000 r--p 0000a000 fc:01 6293131                    /lib/x86_64-linux-gnu/libnss_files-2.19.so\n"
    "7fbca89d4000-7fbca89d5000 rw-p 0000b000 fc:01 6293131                    /lib/x86_64-linux-gnu/libnss_files-2.19.so\n"
    "7fbca89d5000-7fbca89e0000 r-xp 00000000 fc:01 6293133                    /lib/x86_64-linux-gnu/libnss_nis-2.19.so\n"
    "7fbca89e0000-7fbca8bdf000 ---p 0000b000 fc:01 6293133                    /lib/x86_64-linux-gnu/libnss_nis-2.19.so\n"
    "7fbca8bdf000-7fbca8be0000 r--p 0000a000 fc:01 6293133                    /lib/x86_64-linux-gnu/libnss_nis-2.19.so\n"
    "7fbca8be0000-7fbca8be1000 rw-p 0000b000 fc:01 6293133                    /lib/x86_64-linux-gnu/libnss_nis-2.19.so\n"
    "7fbca8be1000-7fbca8bf8000 r-xp 00000000 fc:01 6293114                    /lib/x86_64-linux-gnu/libnsl-2.19.so\n"
    "7fbca8bf8000-7fbca8df7000 ---p 00017000 fc:01 6293114                    /lib/x86_64-linux-gnu/libnsl-2.19.so\n"
    "7fbca8df7000-7fbca8df8000 r--p 00016000 fc:01 6293114                    /lib/x86_64-linux-gnu/libnsl-2.19.so\n"
    "7fbca8df8000-7fbca8df9000 rw-p 00017000 fc:01 6293114                    /lib/x86_64-linux-gnu/libnsl-2.19.so\n"
    "7fbca8df9000-7fbca8dfb000 rw-p 00000000 00:00 0 \n"
    "7fbca8dfb000-7fbca8e04000 r-xp 00000000 fc:01 6293104                    /lib/x86_64-linux-gnu/libnss_compat-2.19.so\n"
    "7fbca8e04000-7fbca9003000 ---p 00009000 fc:01 6293104                    /lib/x86_64-linux-gnu/libnss_compat-2.19.so\n"
    "7fbca9003000-7fbca9004000 r--p 00008000 fc:01 6293104                    /lib/x86_64-linux-gnu/libnss_compat-2.19.so\n"
    "7fbca9004000-7fbca9005000 rw-p 00009000 fc:01 6293104                    /lib/x86_64-linux-gnu/libnss_compat-2.19.so\n"
    "7fbca9005000-7fbca92ce000 r--p 12300000 fc:01 6044294                    /usr/lib/locale/locale-archive\n"
    "7fbca92ce000-7fbca9489000 r-xp 00000000 fc:01 6293122                    /lib/x86_64-linux-gnu/libc-2.19.so\n"
    "7fbca9489000-7fbca9689000 ---p 001bb000 fc:01 6293122                    /lib/x86_64-linux-gnu/libc-2.19.so\n"
    "7fbca9689000-7fbca968d000 r--p 001bb000 fc:01 6293122                    /lib/x86_64-linux-gnu/libc-2.19.so\n"
    "7fbca968d000-7fbca968f000 rw-p 001bf000 fc:01 6293122                    /lib/x86_64-linux-gnu/libc-2.19.so\n"
    "7fbca968f000-7fbca9694000 rw-p 00000000 00:00 0 \n"
    "7fbca9694000-7fbca9697000 r-xp 00000000 fc:01 6293141                    /lib/x86_64-linux-gnu/libdl-2.19.so\n"
    "7fbca9697000-7fbca9896000 ---p 00003000 fc:01 6293141                    /lib/x86_64-linux-gnu/libdl-2.19.so\n"
    "7fbca9896000-7fbca9897000 r--p 00002000 fc:01 6293141                    /lib/x86_64-linux-gnu/libdl-2.19.so\n"
    "7fbca9897000-7fbca9898000 rw-p 00003000 fc:01 6293141                    /lib/x86_64-linux-gnu/libdl-2.19.so\n"
    "7fbca9898000-7fbca98bd000 r-xp 00000000 fc:01 6294567                    /lib/x86_64-linux-gnu/libtinfo.so.5.9\n"
    "7fbca98bd000-7fbca9abc000 ---p 00025000 fc:01 6294567                    /lib/x86_64-linux-gnu/libtinfo.so.5.9\n"
    "7fbca9abc000-7fbca9ac0000 r--p 00024000 fc:01 6294567                    /lib/x86_64-linux-gnu/libtinfo.so.5.9\n"
    "7fbca9ac0000-7fbca9ac1000 rw-p 00028000 fc:01 6294567                    /lib/x86_64-linux-gnu/libtinfo.so.5.9\n"
    "7fbca9ac1000-7fbca9ae4000 r-xp 00000000 fc:01 6293124                    /lib/x86_64-linux-gnu/ld-2.19.so\n"
    "7fbca9cb3000-7fbca9cb6000 rw-p 00000000 00:00 0 \n"
    "7fbca9cda000-7fbca9ce1000 r--s 00000000 fc:01 6705529                    /usr/lib/x86_64-linux-gnu/gconv/gconv-modules.cache\n"
    "7fbca9ce1000-7fbca9ce3000 rw-p 00000000 00:00 0 \n"
    "7fbca9ce3000-7fbca9ce4000 r--p 00022000 fc:01 6293124                    /lib/x86_64-linux-gnu/ld-2.19.so\n"
    "7fbca9ce4000-7fbca9ce5000 rw-p 00023000 fc:01 6293124                    /lib/x86_64-linux-gnu/ld-2.19.so\n"
    "7fbca9ce5000-7fbca9ce6000 rw-p 00000000 00:00 0 \n"
    "7fff944b0000-7fff944d1000 rw-p 00000000 00:00 0                          [stack]\n"
    "7fff945fe000-7fff94600000 r-xp 00000000 00:00 0                          [vdso]\n"
    "ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]\n";


TEST(ProcMaps, GetRealSnapshotTest) {
  auto map = memorymap_snapshot(getpid());
  SCOPE_EXIT {
    memorymap_destroy(map);
  };

  EXPECT_NE(map, nullptr);
}

TEST(ProcMaps, UseFakeSnapshotTest) {
  procmaps_test_snapshot = test_data;
  SCOPE_EXIT {
    procmaps_test_snapshot = nullptr;
  };

  auto map = memorymap_snapshot(getpid());
  SCOPE_EXIT {
    memorymap_destroy(map);
  };

  ASSERT_NE(map, nullptr);

  int i = 0;
  for (auto* vma = memorymap_first_vma(map);
       vma;
       vma = memorymap_vma_next(vma)) {
    i++;
    EXPECT_NE(memorymap_vma_file(vma), nullptr);
    EXPECT_NE(memorymap_vma_permissions(vma), nullptr);
  }
  EXPECT_EQ(i, 46);

  auto* vma = memorymap_find(map, 0x7fbca9006012); // in locale-archive
  ASSERT_NE(vma, nullptr);
  EXPECT_EQ(memorymap_vma_start(vma), 0x7fbca9005000);
  EXPECT_EQ(memorymap_vma_end(vma), 0x7fbca92ce000);
  EXPECT_EQ(memorymap_vma_offset(vma), 0x12300000);
  EXPECT_STREQ(memorymap_vma_permissions(vma), "r--p");
  EXPECT_STREQ(memorymap_vma_file(vma), "/usr/lib/locale/locale-archive");

  vma = memorymap_find(map, 0xffffffffff600016);
  ASSERT_NE(vma, nullptr);
  EXPECT_STREQ(memorymap_vma_file(vma), "[vsyscall]");

  vma = memorymap_find(map, 0x00400000);
  ASSERT_NE(vma, nullptr);
  EXPECT_STREQ(memorymap_vma_file(vma), "/bin/bash");
  EXPECT_STREQ(memorymap_vma_permissions(vma), "r-xp");
}

TEST(ProcMaps, CanFreeNull) {
  memorymap_destroy(nullptr);
}

TEST(ProcMaps, CannotGetOthersMaps) {
  EXPECT_EQ(memorymap_snapshot(1), nullptr);
}