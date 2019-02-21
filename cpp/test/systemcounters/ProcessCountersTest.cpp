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

struct TestTaskStatFile {
  util::TaskStatInfo prevStats;
  util::TaskStatInfo stats;

  TestTaskStatFile(std::string path) : prevStats(), stats() {}

  util::TaskStatInfo getInfo() {
    return prevStats;
  }

  util::TaskStatInfo refresh(uint32_t requested_stats_mask = 0) {
    return stats;
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
  }

  void testCounters(
      uint32_t test_stats_mask,
      uint32_t expected_cur_stats_mask,
      int32_t cur_value = kCurValue,
      int32_t prev_value = kPrevValue) {
    std::unique_ptr<TestTaskStatFile> statFile =
        std::unique_ptr<TestTaskStatFile>(new TestTaskStatFile(""));
    std::unique_ptr<TestTaskSchedFile> schedFile =
        std::unique_ptr<TestTaskSchedFile>(new TestTaskSchedFile(""));

    schedFile->availableStatsMask = test_stats_mask;

    schedFile->prevStats.nrVoluntarySwitches = prev_value;
    schedFile->prevStats.nrInvoluntarySwitches = prev_value;
    schedFile->prevStats.iowaitSum = prev_value;
    schedFile->prevStats.iowaitCount = prev_value;
    statFile->prevStats.cpuTime = prev_value;
    statFile->prevStats.majorFaults = prev_value;
    statFile->prevStats.kernelCpuTimeMs = prev_value;
    statFile->prevStats.minorFaults = prev_value;

    schedFile->stats.nrVoluntarySwitches = cur_value;
    schedFile->stats.nrInvoluntarySwitches = cur_value;
    schedFile->stats.iowaitSum = cur_value;
    schedFile->stats.iowaitCount = cur_value;
    statFile->stats.cpuTime = cur_value;
    statFile->stats.majorFaults = cur_value;
    statFile->stats.kernelCpuTimeMs = cur_value;
    statFile->stats.minorFaults = cur_value;

    std::unordered_set<int32_t> expectedStatTypesCur;
    fillQuickLogStatsSetByMask(expected_cur_stats_mask, expectedStatTypesCur);

    ProcessCounters<TestTaskStatFile, TestTaskSchedFile, TestLogger>
        processCounters{std::move(statFile), std::move(schedFile)};

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
      EXPECT_EQ(logEntry.extra, kCurValue);
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
