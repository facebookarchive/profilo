/**
 * Copyright 2018-present, Facebook, Inc.
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

#include <profilo/systemcounters/ProcessCounters.h>
#include <profilo/LogEntry.h>
#include <util/ProcFs.h>

#include <gtest/gtest.h>

#include <stack>
#include <unordered_map>

using facebook::profilo::entries::StandardEntry;
using facebook::profilo::util::SchedInfo;
using facebook::profilo::util::StatType;
using facebook::profilo::util::TaskStatInfo;

namespace facebook {
namespace profilo {

constexpr int32_t kAllStatsMask = 0xffffffff;

#define EXPECT_SET_CONTAINS(value, set) \
  EXPECT_PRED_FORMAT2(assertContainsInSet<int32_t>, value, set)

template <typename Key>
static ::testing::AssertionResult assertContainsInSet(
    const char* key_code_str,
    const char* set_code_str,
    const Key key,
    const std::unordered_set<Key>& set) {
  if (set.find(key) == set.end()) {
    return ::testing::AssertionFailure()
        << key_code_str << "=" << key
        << " is not found in set: " << set_code_str;
  }
  return ::testing::AssertionSuccess();
}

struct TestLogger {
  std::stack<StandardEntry> log;

  static TestLogger& get() {
    static TestLogger instance{};
    return instance;
  }

  int32_t write(StandardEntry&& entry, uint16_t id_step = 1) {
    log.push(entry);
    return 0;
  }
};

struct TestTaskSchedFile {
  util::SchedInfo prevStats;
  util::SchedInfo stats;
  int32_t availableStatsMask;

  TestTaskSchedFile(std::string path)
      : prevStats(), stats(), availableStatsMask() {}

  util::SchedInfo getInfo() {
    return prevStats;
  }

  util::SchedInfo refresh(uint32_t requested_stats_mask = 0) {
    return stats;
  }
};

struct TestProcStatmFile {
  util::StatmInfo prevStats;
  util::StatmInfo stats;
  int32_t availableStatsMask;

  TestProcStatmFile() = default;

  util::StatmInfo getInfo() {
    return prevStats;
  }

  util::StatmInfo refresh(uint32_t requested_stats_mask = 0) {
    return stats;
  }
};

struct TestGetRusageStatsProvider {
  TestGetRusageStatsProvider() = default;

  TestGetRusageStatsProvider(int prev_value, int cur_value)
      : prevStats(), curStats() {
    prevStats.ru_utime.tv_usec = prev_value * 1000;
    prevStats.ru_stime.tv_usec = prev_value * 1000;
    prevStats.ru_majflt = prev_value;
    prevStats.ru_minflt = prev_value;

    curStats.ru_utime.tv_usec = cur_value * 1000;
    curStats.ru_stime.tv_usec = cur_value * 1000;
    curStats.ru_majflt = cur_value;
    curStats.ru_minflt = cur_value;
  }

  void refresh() {
    // no-op
  }

  rusage prevStats;
  rusage curStats;
};

class ProcessCountersTestAccessor {
 public:
  explicit ProcessCountersTestAccessor(ProcessCounters<
                                       TestTaskSchedFile,
                                       TestLogger,
                                       TestGetRusageStatsProvider,
                                       TestProcStatmFile>& processCounters)
      : processCounters_(processCounters) {}

  void substituteSchedFile(std::unique_ptr<TestTaskSchedFile>&& schedFile) {
    processCounters_.schedStats_ = std::move(schedFile);
  }

  void substituteGetRusageStatsProvider(
      TestGetRusageStatsProvider&& getRusageProvider) {
    processCounters_.getRusageStats_ = std::move(getRusageProvider);
  }

 private:
  ProcessCounters<
      TestTaskSchedFile,
      TestLogger,
      TestGetRusageStatsProvider,
      TestProcStatmFile>& processCounters_;
};

class ProcessMonotonicCountersTest : public ::testing::Test {
 protected:
  static constexpr int32_t kPrevValue = 10;
  static constexpr int32_t kCurValue = 20;

  ProcessMonotonicCountersTest()
      : ::testing::Test(), testLogger(TestLogger::get()) {}
  virtual ~ProcessMonotonicCountersTest() = default;

  static void fillQuickLogStatsSetByMask(
      int32_t stats_mask,
      std::unordered_set<int32_t>& quickLogSet) {
    if (StatType::CPU_TIME & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_CPU_TIME);
    }
    if (StatType::MAJOR_FAULTS & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_SW_FAULTS_MAJOR);
    }
    if (StatType::NR_VOLUNTARY_SWITCHES & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_CONTEXT_SWITCHES_VOLUNTARY);
    }
    if (StatType::NR_INVOLUNTARY_SWITCHES & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_CONTEXT_SWITCHES_INVOLUNTARY);
    }
    if (StatType::IOWAIT_SUM & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_IOWAIT_TIME);
    }
    if (StatType::IOWAIT_COUNT & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_IOWAIT_COUNT);
    }
    if (StatType::MINOR_FAULTS & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_SW_FAULTS_MINOR);
    }
    if (StatType::KERNEL_CPU_TIME & stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_KERNEL_CPU_TIME);
    }
    if (StatType::STATM_SHARED && stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_STATM_SHARED);
    }
    if (StatType::STATM_RESIDENT && stats_mask) {
      quickLogSet.insert(QuickLogConstants::PROC_STATM_RESIDENT);
    }
  }

  void testCounters(
      uint32_t test_stats_mask,
      uint32_t expected_cur_stats_mask,
      int32_t cur_value = kCurValue,
      int32_t prev_value = kPrevValue) {
    std::unique_ptr<TestTaskSchedFile> schedFile =
        std::unique_ptr<TestTaskSchedFile>(new TestTaskSchedFile(""));

    schedFile->availableStatsMask = test_stats_mask;

    schedFile->prevStats.nrVoluntarySwitches = prev_value;
    schedFile->prevStats.nrInvoluntarySwitches = prev_value;
    schedFile->prevStats.iowaitSum = prev_value;
    schedFile->prevStats.iowaitCount = prev_value;

    schedFile->stats.nrVoluntarySwitches = cur_value;
    schedFile->stats.nrInvoluntarySwitches = cur_value;
    schedFile->stats.iowaitSum = cur_value;
    schedFile->stats.iowaitCount = cur_value;

    std::unique_ptr<TestProcStatmFile> statmFile =
        std::unique_ptr<TestProcStatmFile>(new TestProcStatmFile());

    statmFile->availableStatsMask = test_stats_mask;
    statmFile->prevStats.resident = prev_value;
    statmFile->prevStats.shared = prev_value;
    statmFile->stats.resident = cur_value;
    statmFile->stats.shared = cur_value;

    std::unordered_set<int32_t> expectedStatTypesCur;
    fillQuickLogStatsSetByMask(expected_cur_stats_mask, expectedStatTypesCur);

    ProcessCounters<
        TestTaskSchedFile,
        TestLogger,
        TestGetRusageStatsProvider,
        TestProcStatmFile>
        processCounters{};

    ProcessCountersTestAccessor processCountersAccessor(processCounters);
    processCountersAccessor.substituteSchedFile(std::move(schedFile));
    processCountersAccessor.substituteGetRusageStatsProvider(
        TestGetRusageStatsProvider(prev_value, cur_value));

    auto timeBeforeLogging = monotonicTime();
    processCounters.logCounters();

    if (expected_cur_stats_mask != 0) {
      EXPECT_EQ(testLogger.log.empty(), false);
    }

    while (!testLogger.log.empty()) {
      StandardEntry logEntry = testLogger.log.top();
      auto timestamp = logEntry.timestamp;
      EXPECT_GT(timestamp, timeBeforeLogging);
      EXPECT_SET_CONTAINS(logEntry.callid, expectedStatTypesCur);
      if (logEntry.callid == QuickLogConstants::PROC_CPU_TIME) {
        // This is an exception as cpu time is get by summing utime + stime from
        // "getrusage", so we need to account for that.
        EXPECT_EQ(logEntry.extra, 2 * kCurValue);
      } else {
        EXPECT_EQ(logEntry.extra, kCurValue);
      }
      expectedStatTypesCur.erase(logEntry.callid);
      testLogger.log.pop();
    }
  }

  TestLogger& testLogger;
};

constexpr int32_t ProcessMonotonicCountersTest::kPrevValue;
constexpr int32_t ProcessMonotonicCountersTest::kCurValue;

TEST_F(ProcessMonotonicCountersTest, testCountersMoved) {
  testCounters(kAllStatsMask, kAllStatsMask, kCurValue, kPrevValue);
}

TEST_F(ProcessMonotonicCountersTest, testCountersAreNotLoggedIfNotMoved) {
  testCounters(kAllStatsMask, 0 /* no stats expected */, 10, 10);
}
} // namespace profilo
} // namespace facebook
