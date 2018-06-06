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

#include <folly/experimental/TestUtil.h>
#include <gtest/gtest.h>

#include <fstream>

#include <util/ProcFs.h>
#include <util/common.h>

namespace fs = boost::filesystem;
namespace test = folly::test;

namespace facebook {
namespace profilo {
namespace util {

static const auto ALL_STATS_MASK = 0xffffffff;

static const std::string STAT_CONTENT =
    "4653 (ebook.wakizashi) R 671 670 0 0 -1 4211008 22794 0 553 0 104 32 0 0 12 -8 32 0 147144270 2242772992 46062 18446744073709551615 2909851648 2909870673 4288193888 4288148192 4077764852 0 4612 0 1098945784 0 0 0 17 2 0 0 0 0 0 2909875376 2909876224 2913968128 4288195386 4288195495 4288195495 4288196574 0";
static const std::string SCHEDSTAT_CONTENT = "2075550186 1196266356 3934";
static const std::string SCHED_CONTENT =
    "procfs_sched (29376, #threads: 1) \n"
    "-------------------------------------------------------------------\n"
    "se.exec_start                                :     115728485.820777\n"
    "se.vruntime                                  :      52304885.848107\n"
    "se.sum_exec_runtime                          :            43.046463\n"
    "se.statistics.iowait_sum                     :            21.256929\n"
    "se.statistics.iowait_count                   :                   14\n"
    "avg_per_cpu                                  :            21.523231\n"
    "nr_switches                                  :                   60\n"
    "nr_voluntary_switches                        :                   14\n"
    "nr_involuntary_switches                      :                   46\n"
    "prio                                         :                  120\n"
    "clock-delta                                  :                  573\n";
static const std::string VMSTAT_CONTENT =
    "nr_free_pages 123\n"
    "nr_dirty 74317\n"
    "nr_mapped 76672\n"
    "nr_file_pages 235292\n"
    "nr_writeback 17\n"
    "nr_unstable 0\n"
    "nr_bounce 0\n"
    "pgpgin 862085\n"
    "pgpgout 133892\n"
    "pgmajfault 109\n"
    "pgfree 978797\n"
    "kswapd_steal 101\n"
    "kswapd_low_wmark_hit_quickly 0\n"
    "kswapd_high_wmark_hit_quickly 0\n"
    "pageoutrun 12\n"
    "allocstall 3\n";
static const std::string VMSTAT_CONTENT_MODIFIED =
    "nr_free_pages 123\n"
    "nr_dirty 74317\n"
    "nr_mapped 76672\n"
    "nr_file_pages 235292\n"
    "nr_writeback 1\n"
    "nr_unstable 0\n"
    "nr_bounce 0\n"
    "pgpgin 1857940636\n"
    "pgpgout 133892\n"
    "pgmajfault 109\n"
    "pgfree 978797\n"
    "kswapd_steal 101\n"
    "kswapd_low_wmark_hit_quickly 0\n"
    "kswapd_high_wmark_hit_quickly 0\n"
    "pageoutrun 12\n"
    "allocstall 32\n";
static const std::string VMSTAT_CONTENT_WITH_SPLIT_KSWAPD =
    "nr_free_pages 123\n"
    "pgsteal_kswapd_dma 236448262\n"
    "pgsteal_kswapd_normal 228999324\n"
    "pgsteal_kswapd_movable 1\n"
    "pageoutrun 12\n"
    "allocstall 3\n";

class ProcFsTest : public ::testing::Test {
 protected:
  ProcFsTest() : ::testing::Test(), temp_stat_file_("test_stat") {}

  fs::path SetUpTempFile(const std::string& stat_content) {
    std::ofstream statFileStream(temp_stat_file_.path().c_str());
    statFileStream << stat_content;
    return temp_stat_file_.path();
  }

  test::TemporaryFile temp_stat_file_;
};

TEST_F(ProcFsTest, testSchedFile) {
  fs::path statPath = SetUpTempFile(SCHED_CONTENT);
  TaskSchedFile schedFile{statPath.native()};
  SchedInfo schedInfo = schedFile.refresh(ALL_STATS_MASK);

  EXPECT_EQ(schedInfo.iowaitCount, 14);
  EXPECT_EQ(schedInfo.iowaitSum, 21);
  EXPECT_EQ(schedInfo.nrVoluntarySwitches, 14);
  EXPECT_EQ(schedInfo.nrInvoluntarySwitches, 46);
}

TEST_F(ProcFsTest, testSchedStatFile) {
  fs::path statPath = SetUpTempFile(SCHEDSTAT_CONTENT);
  TaskSchedstatFile schedStatFile{statPath.native()};
  SchedstatInfo statInfo = schedStatFile.refresh(ALL_STATS_MASK);

  EXPECT_EQ(statInfo.cpuTimeMs, 2075);
  EXPECT_EQ(statInfo.waitToRunTimeMs, 1196);
}

TEST_F(ProcFsTest, testStatFile) {
  fs::path statPath = SetUpTempFile(STAT_CONTENT);
  TaskStatFile statFile{statPath.native()};
  TaskStatInfo statInfo = statFile.refresh(ALL_STATS_MASK);

  static const auto CLK_TICK = systemClockTickIntervalMs();

  EXPECT_EQ(statInfo.state, TS_RUNNING);
  EXPECT_EQ(statInfo.minorFaults, 22794);
  EXPECT_EQ(statInfo.majorFaults, 553);
  EXPECT_EQ(statInfo.kernelCpuTimeMs, 32 * CLK_TICK);
  EXPECT_EQ(statInfo.cpuTime, 136 * CLK_TICK);
  EXPECT_EQ(statInfo.cpuNum, 2);
}

TEST_F(ProcFsTest, testVmStatFile) {
  fs::path statPath = SetUpTempFile(VMSTAT_CONTENT);
  VmStatFile statFile {statPath.native()};
  VmStatInfo statInfo = statFile.refresh(ALL_STATS_MASK);

  EXPECT_EQ(statInfo.nrFreePages, 123);
  EXPECT_EQ(statInfo.nrDirty, 74317);
  EXPECT_EQ(statInfo.nrWriteback, 17);
  EXPECT_EQ(statInfo.pgPgIn, 862085);
  EXPECT_EQ(statInfo.pgPgOut, 133892);
  EXPECT_EQ(statInfo.pgMajFault, 109);
  EXPECT_EQ(statInfo.allocStall, 3);
  EXPECT_EQ(statInfo.pageOutrun, 12);
  EXPECT_EQ(statInfo.kswapdSteal, 101);

  SetUpTempFile(VMSTAT_CONTENT_MODIFIED);
  statInfo = statFile.refresh(ALL_STATS_MASK);
  EXPECT_EQ(statInfo.nrFreePages, 123);
  EXPECT_EQ(statInfo.nrDirty, 74317);
  EXPECT_EQ(statInfo.nrWriteback, 1);
  EXPECT_EQ(statInfo.pgPgIn, 1857940636);
  EXPECT_EQ(statInfo.pgPgOut, 133892);
  EXPECT_EQ(statInfo.pgMajFault, 109);
  EXPECT_EQ(statInfo.allocStall, 32);
  EXPECT_EQ(statInfo.pageOutrun, 12);
  EXPECT_EQ(statInfo.kswapdSteal, 101);
}

TEST_F(ProcFsTest, testVmStatFileWithSplitKswapd) {
  fs::path statPath = SetUpTempFile(VMSTAT_CONTENT_WITH_SPLIT_KSWAPD);
  VmStatFile statFile {statPath.native()};
  VmStatInfo statInfo = statFile.refresh(ALL_STATS_MASK);

  EXPECT_EQ(statInfo.nrFreePages, 123);
  EXPECT_EQ(statInfo.allocStall, 3);
  EXPECT_EQ(statInfo.pageOutrun, 12);
  EXPECT_EQ(statInfo.kswapdSteal, 465447587);
}

} // namespace util
} // namespace profilo
} // namespace facebook
