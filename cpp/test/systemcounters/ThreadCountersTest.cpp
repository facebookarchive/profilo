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

#include <profilo/systemcounters/ThreadCounters.h>
#include <profilo/LogEntry.h>
#include <util/ProcFs.h>

#include <gtest/gtest.h>

#include <stack>
#include <unordered_map>

using facebook::profilo::entries::StandardEntry;
using facebook::profilo::util::stats_callback_fn;
using facebook::profilo::util::StatType;
using facebook::profilo::util::ThreadStatInfo;

namespace facebook {
namespace profilo {

constexpr int32_t kAllStatsMask = 0xffffffff;
constexpr int32_t kAllStatsNoHiFreqCpuTime =
    kAllStatsMask & (~StatType::HIGH_PRECISION_CPU_TIME);

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

struct TestThreadCache {
  ThreadStatInfo prevStats;
  ThreadStatInfo stats;

  void forEach(
      stats_callback_fn callback,
      uint32_t requested_stats_mask,
      const std::unordered_set<int32_t>* black_list) {
    callback(1, prevStats, stats);
  }

  void forThread(
      uint32_t tid,
      stats_callback_fn callback,
      uint32_t requested_stats_mask) {
    callback(1, prevStats, stats);
  }

  int32_t getStatsAvailabililty(int32_t tid) {
    return kAllStatsMask;
  }
};

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

class ThreadMonotonicCountersTest : public ::testing::Test {
 protected:
  static constexpr int32_t kPrevTime = 1000;
  static constexpr int32_t kPrevValue = 10;
  static constexpr int32_t kCurTime = 2000;
  static constexpr int32_t kCurValue = 20;

  ThreadMonotonicCountersTest()
      : ::testing::Test(), cache(), testLogger(TestLogger::get()) {}
  virtual ~ThreadMonotonicCountersTest() = default;

  static void fillQuickLogStatsSetByMask(
      int32_t stats_mask,
      std::unordered_set<int32_t>& quickLogSet) {
    if ((StatType::CPU_TIME & stats_mask) ||
        (StatType::HIGH_PRECISION_CPU_TIME & stats_mask)) {
      quickLogSet.insert(QuickLogConstants::THREAD_CPU_TIME);
    }
    if (StatType::MAJOR_FAULTS & stats_mask) {
      quickLogSet.insert(QuickLogConstants::QL_THREAD_FAULTS_MAJOR);
    }
    if (StatType::WAIT_TO_RUN_TIME & stats_mask) {
      quickLogSet.insert(QuickLogConstants::THREAD_WAIT_IN_RUNQUEUE_TIME);
    }
    if (StatType::NR_VOLUNTARY_SWITCHES & stats_mask) {
      quickLogSet.insert(QuickLogConstants::CONTEXT_SWITCHES_VOLUNTARY);
    }
    if (StatType::NR_INVOLUNTARY_SWITCHES & stats_mask) {
      quickLogSet.insert(QuickLogConstants::CONTEXT_SWITCHES_INVOLUNTARY);
    }
    if (StatType::IOWAIT_SUM & stats_mask) {
      quickLogSet.insert(QuickLogConstants::IOWAIT_TIME);
    }
    if (StatType::IOWAIT_COUNT & stats_mask) {
      quickLogSet.insert(QuickLogConstants::IOWAIT_COUNT);
    }
    if (StatType::MINOR_FAULTS & stats_mask) {
      quickLogSet.insert(QuickLogConstants::THREAD_SW_FAULTS_MINOR);
    }
    if (StatType::KERNEL_CPU_TIME & stats_mask) {
      quickLogSet.insert(QuickLogConstants::THREAD_KERNEL_CPU_TIME);
    }
  }

  void testCounters(
      uint32_t test_stats_mask,
      bool prev_stat_changed,
      uint32_t expected_cur_stats_mask,
      uint32_t expected_prev_stats_mask,
      int32_t cur_value = kCurValue,
      int32_t prev_value = kPrevValue) {
    cache.prevStats.monotonicStatTime = kPrevTime;
    cache.prevStats.statChangeMask = prev_stat_changed ? test_stats_mask : 0;
    cache.prevStats.availableStatsMask = test_stats_mask;
    cache.prevStats.cpuTimeMs = prev_value;
    cache.prevStats.highPrecisionCpuTimeMs = prev_value;
    cache.prevStats.waitToRunTimeMs = prev_value;
    cache.prevStats.majorFaults = prev_value;
    cache.prevStats.nrVoluntarySwitches = prev_value;
    cache.prevStats.nrInvoluntarySwitches = prev_value;
    cache.prevStats.iowaitSum = prev_value;
    cache.prevStats.iowaitCount = prev_value;
    cache.prevStats.kernelCpuTimeMs = prev_value;
    cache.prevStats.minorFaults = prev_value;

    cache.stats.monotonicStatTime = kCurTime;
    cache.stats.availableStatsMask = test_stats_mask;
    cache.stats.cpuTimeMs = cur_value;
    cache.stats.highPrecisionCpuTimeMs = cur_value;
    cache.stats.waitToRunTimeMs = cur_value;
    cache.stats.majorFaults = cur_value;
    cache.stats.nrVoluntarySwitches = cur_value;
    cache.stats.nrInvoluntarySwitches = cur_value;
    cache.stats.iowaitSum = cur_value;
    cache.stats.iowaitCount = cur_value;
    cache.stats.kernelCpuTimeMs = cur_value;
    cache.stats.minorFaults = cur_value;

    std::unordered_set<int32_t> expectedStatTypesCur;
    std::unordered_set<int32_t> expectedStatTypesPrev;

    fillQuickLogStatsSetByMask(expected_cur_stats_mask, expectedStatTypesCur);
    fillQuickLogStatsSetByMask(expected_prev_stats_mask, expectedStatTypesPrev);

    ThreadCounters<TestThreadCache, TestLogger> threadCounters{cache};
    std::unordered_set<int32_t> ignored_tids{};
    threadCounters.logCounters(false, ignored_tids);

    if (expected_cur_stats_mask != 0 || expected_prev_stats_mask != 0) {
      EXPECT_EQ(testLogger.log.empty(), false);
    }

    while (!testLogger.log.empty()) {
      StandardEntry logEntry = testLogger.log.top();
      auto timestamp = logEntry.timestamp;
      switch (timestamp) {
        case kPrevTime: {
          EXPECT_SET_CONTAINS(logEntry.callid, expectedStatTypesPrev);
          EXPECT_EQ(logEntry.extra, kPrevValue);
          expectedStatTypesPrev.erase(logEntry.callid);
          break;
        }
        case kCurTime: {
          EXPECT_SET_CONTAINS(logEntry.callid, expectedStatTypesCur);
          EXPECT_EQ(logEntry.extra, kCurValue);
          expectedStatTypesCur.erase(logEntry.callid);
          break;
        }
        default:
          FAIL() << "Unexpected time for log entry";
      }
      testLogger.log.pop();
    }
  }

  TestThreadCache cache;
  TestLogger& testLogger;
};

constexpr int32_t ThreadMonotonicCountersTest::kPrevTime;
constexpr int32_t ThreadMonotonicCountersTest::kPrevValue;
constexpr int32_t ThreadMonotonicCountersTest::kCurTime;
constexpr int32_t ThreadMonotonicCountersTest::kCurValue;

/**
 * Scenario where stats move, but previous sample had counters moved and logged
 * too. Expect the previous point won't be logged twice.
 */
TEST_F(ThreadMonotonicCountersTest, testCountersMoveAndPreviousPointMovedToo) {
  testCounters(kAllStatsNoHiFreqCpuTime, true, kAllStatsNoHiFreqCpuTime, 0);
}

/**
 * The same as above but for high precision cpu time, as it's a special case.
 */
TEST_F(
    ThreadMonotonicCountersTest,
    testCountersMoveAndPreviousPointMovedTooForHiPrecCpuTime) {
  testCounters(
      StatType::HIGH_PRECISION_CPU_TIME,
      true,
      StatType::HIGH_PRECISION_CPU_TIME,
      0);
}

/**
 * Scenario where stats move, but previous sample was skipped due to no change.
 * Expect the previous point to be logged in this case.
 */
TEST_F(ThreadMonotonicCountersTest, testCountersMoveWithPreviousPointSkipped) {
  testCounters(
      kAllStatsNoHiFreqCpuTime,
      false,
      kAllStatsNoHiFreqCpuTime,
      kAllStatsNoHiFreqCpuTime);
}

/**
 * The same as above but for high precision cpu time, as it's a special case.
 */
TEST_F(
    ThreadMonotonicCountersTest,
    testCountersMoveWithPreviousPointSkippedForHiPrecCputTime) {
  testCounters(
      StatType::HIGH_PRECISION_CPU_TIME,
      false,
      StatType::HIGH_PRECISION_CPU_TIME,
      StatType::HIGH_PRECISION_CPU_TIME);
}

TEST_F(ThreadMonotonicCountersTest, testCountersAreNotLoggedIfNotMoved) {
  testCounters(kAllStatsMask, false, 0, 0, 10, 10);
}

} // namespace profilo
} // namespace facebook
