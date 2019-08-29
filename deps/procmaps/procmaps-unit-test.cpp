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
#include <sys/user.h>
#include <limits>

#include <folly/ScopeGuard.h>
#include <system_error>
#include <thread>
#include <cstdio>

#include "procmaps.c" // C file!!

template <typename T>
class SataddTest : public ::testing::Test {};

TYPED_TEST_CASE_P(SataddTest);

TYPED_TEST_P(SataddTest, TestZero) {
  TypeParam a { 0 };
  TypeParam b { 0 };
  TypeParam r {};

  EXPECT_FALSE(SATADD(&r, a, b));
  EXPECT_EQ(r, 0);
}

TYPED_TEST_P(SataddTest, TestSmall) {
  TypeParam a { 4 };
  TypeParam b { 4 };
  TypeParam r {};

  EXPECT_FALSE(SATADD(&r, a, b));
  EXPECT_EQ(r, 8);
}

TYPED_TEST_P(SataddTest, TestNoOverflow) {
  TypeParam a { std::numeric_limits<TypeParam>::max() / 2 };
  TypeParam b { std::numeric_limits<TypeParam>::max() / 2 };
  TypeParam r {};

  EXPECT_FALSE(SATADD(&r, a, b));
  EXPECT_EQ(r, std::numeric_limits<TypeParam>::max() - 1);
}

TYPED_TEST_P(SataddTest, TestExact) {
  TypeParam a { std::numeric_limits<TypeParam>::max() / 2 + 1};
  TypeParam b { std::numeric_limits<TypeParam>::max() / 2 };
  TypeParam r {};

  EXPECT_FALSE(SATADD(&r, a, b));
  EXPECT_EQ(r, std::numeric_limits<TypeParam>::max());
}

TYPED_TEST_P(SataddTest, TestBarelyOverflow) {
  TypeParam a { std::numeric_limits<TypeParam>::max() / 2 + 1};
  TypeParam b { std::numeric_limits<TypeParam>::max() / 2 + 1};
  TypeParam r {};

  EXPECT_TRUE(SATADD(&r, a, b));
  EXPECT_EQ(r, std::numeric_limits<TypeParam>::max());
}

TYPED_TEST_P(SataddTest, TestMassiveOverflow) {
  TypeParam a { std::numeric_limits<TypeParam>::max() };
  TypeParam b { std::numeric_limits<TypeParam>::max() };
  TypeParam r {};

  EXPECT_TRUE(SATADD(&r, a, b));
  EXPECT_EQ(r, std::numeric_limits<TypeParam>::max());
}


REGISTER_TYPED_TEST_CASE_P(
    SataddTest,
    TestZero,
    TestSmall,
    TestNoOverflow,
    TestExact,
    TestBarelyOverflow,
    TestMassiveOverflow);
typedef testing::Types<uint8_t, uint32_t, uint64_t> SataddTypes;
INSTANTIATE_TYPED_TEST_CASE_P(SataddTest, SataddTest, SataddTypes);





TEST(AllocTest, FreeNull) {
  EXPECT_EQ(xfree(nullptr), 0);
}

TEST(AllocTest, AllocZero) {
  EXPECT_EQ(xrealloc(nullptr, 0), nullptr);
}

TEST(AllocTest, AllocTooBig) {
  EXPECT_EQ(xrealloc(nullptr, std::numeric_limits<size_t>::max()), nullptr);
}

TEST(AllocTest, FreeBad) {
  auto foo { 5 };
  auto* ptr { &foo };
  if (reinterpret_cast<uintptr_t>(ptr) % PAGE_SIZE == 0) {
    ptr++;
  }

  EXPECT_NE(xfree(ptr), 0);
}

static void allocTest(size_t const sz) {
  auto* alloc = reinterpret_cast<uint8_t*>(xrealloc(nullptr, sz));
  ASSERT_NE(alloc, nullptr);
  SCOPE_EXIT {
    EXPECT_EQ(xfree(alloc), 0);
  };

  for (auto i{0}; i < sz; i++) {
    alloc[i] = 42;
  }
  for (auto i{0}; i < sz; i++) {
    EXPECT_EQ(alloc[i], 42);
  }
}

TEST(AllocTest, AllocSmall) {
  allocTest(16);
}

TEST(AllocTest, AllocLarge) {
  allocTest(16 * 1024);
}

static void reallocTest(size_t const first_sz, size_t const second_sz) {
  auto *alloc = reinterpret_cast<uint8_t *>(xrealloc(nullptr, first_sz));
  ASSERT_NE(alloc, nullptr);
  SCOPE_EXIT {
    EXPECT_EQ(xfree(alloc), 0);
  };

  for (auto i{0}; i < first_sz; i++) {
    alloc[i] = 42;
  }
  for (auto i{0}; i < first_sz; i++) {
    EXPECT_EQ(alloc[i], 42);
  }

  alloc = reinterpret_cast<uint8_t *>(xrealloc(alloc, second_sz));
  ASSERT_NE(alloc, nullptr);
  for (auto i{0}; i < std::min(first_sz, second_sz); i++) {
    EXPECT_EQ(alloc[i], 42);
  }

  for (auto i{0}; i < second_sz; i++) {
    alloc[i] = 24;
  }
  for (auto i{0}; i < second_sz; i++) {
    EXPECT_EQ(alloc[i], 24);
  }
}

TEST(AllocTest, ReallocSmallToLarge) {
  reallocTest(16, 16 * 1024);
}

TEST(AllocTest, ReallocLargeToSmall) {
  reallocTest(16 * 1024, 16);
}

TEST(AllocDeathTest, UseAfterFree) {
  EXPECT_DEATH({
    auto* alloc = reinterpret_cast<uint32_t*>(xrealloc(nullptr, sizeof(uint32_t)));
    *alloc = 42;
    xfree(alloc);
    *alloc += 1;
  }, "");
}

TEST(AllocDeathTest, ReallocZeroFrees) {
  EXPECT_DEATH({
    auto* alloc = reinterpret_cast<uint32_t*>(xrealloc(nullptr, sizeof(uint32_t)));
    *alloc = 42;
    xrealloc(alloc, 0);
    *alloc += 1;
  }, "");
}




constexpr std::array<uint8_t, 4> kWrittenData { 0xFA, 0xCE, 0xB0, 0x0C };
constexpr uint8_t kNeverWrittenData { 42 };
class ReadTest : public ::testing::Test {
public:
  ReadTest() {
    if (pipe(pipe_)) {
      throw std::system_error(errno, std::generic_category());
    }
  }

  ~ReadTest() {
    close(pipe_[0]);
    close(pipe_[1]);
  }

  int readfd() {
    return pipe_[0];
  }

  int writefd() {
    return pipe_[1];
  }

  std::vector<uint8_t> generateAndWriteData(size_t const size, int const fd) {
    std::vector<uint8_t> data(size);
    std::generate(data.begin(), data.end(), [i = 0]() mutable {
      return kWrittenData[i++ % kWrittenData.size()];
    });

    EXPECT_EQ(write(fd, reinterpret_cast<void*>(data.data()), data.size()), size);
    close(writefd());

    return data;
  }

  std::vector<uint8_t> generateAndWriteData(size_t const size) {
    return generateAndWriteData(size, writefd());
  }

  std::pair<std::vector<uint8_t>, int> readData(
      size_t const anticipated_size,
      int const fd,
      int (*read_func)(int, void*, size_t, size_t*) = xread) {
    std::vector<uint8_t> read_data(anticipated_size, kNeverWrittenData);
    size_t read_size {};
    auto ret = read_func(fd, reinterpret_cast<void*>(read_data.data()), read_data.size(), &read_size);

    // verify that anything beyond the read data wasn't touched
    for (auto i {read_size}; i < read_data.size(); i++) {
      EXPECT_EQ(read_data[i], kNeverWrittenData);
    }
    read_data.resize(read_size);

    return std::make_pair(read_data, ret);
  }

  std::pair<std::vector<uint8_t>, int> readData(size_t const anticipated_size) {
    return readData(anticipated_size, readfd());
  }

private:
  int pipe_[2];
};

TEST_F(ReadTest, ExactBuffer) {
  constexpr auto kSize {256};
  auto data = generateAndWriteData(kSize);

  auto read_data = readData(kSize);
  EXPECT_EQ(read_data.second, 0);
  EXPECT_EQ(read_data.first.size(), kSize);
  EXPECT_EQ(read_data.first, data);
}

TEST_F(ReadTest, SmallBuffer) {
  constexpr auto kWriteSize { 256 };
  constexpr auto kReadSize { kWriteSize / 2 };
  auto data = generateAndWriteData(kWriteSize);

  auto read_data = readData(kReadSize);
  EXPECT_EQ(read_data.second, 0);
  EXPECT_EQ(read_data.first.size(), kReadSize);
  EXPECT_TRUE(std::equal(read_data.first.begin(), read_data.first.end(), data.begin()));
}

TEST_F(ReadTest, LargeBuffer) {
  constexpr auto kWriteSize { 256 };
  constexpr auto kReadSize { kWriteSize * 2 };
  auto data = generateAndWriteData(kWriteSize);

  auto read_data = readData(kReadSize);
  EXPECT_EQ(read_data.second, 0);
  EXPECT_EQ(read_data.first.size(), kWriteSize);
  EXPECT_TRUE(std::equal(data.begin(), data.end(), read_data.first.begin()));
}

TEST_F(ReadTest, ZeroBuffer) {
  constexpr auto kWriteSize { 256 };
  constexpr auto kReadSize { 0 };
  auto data = generateAndWriteData(kWriteSize);

  auto read_data = readData(kReadSize);
  EXPECT_EQ(read_data.second, 0);
  EXPECT_EQ(read_data.first.size(), kReadSize);
}

TEST_F(ReadTest, BadFd) {
  auto read_data = readData(256, -1);
  EXPECT_EQ(read_data.second, -1);
  EXPECT_EQ(read_data.first.size(), 0);
}

TEST_F(ReadTest, ClosedFd) {
  close(readfd());
  auto read_data = readData(256);
  EXPECT_EQ(read_data.second, -1);
  EXPECT_EQ(read_data.first.size(), 0);
}

TEST_F(ReadTest, NonReadFd) {
  auto read_data = readData(256, writefd());
  EXPECT_EQ(read_data.second, -1);
  EXPECT_EQ(read_data.first.size(), 0);
}

TEST_F(ReadTest, BadBuffer) {
  constexpr auto kSize {256};
  generateAndWriteData(kSize);

  size_t read_size {};
  EXPECT_EQ(xread(readfd(), nullptr, kSize, &read_size), -1);
  EXPECT_EQ(read_size, 0);
}

TEST_F(ReadTest, AllReadToEOF) {
  constexpr auto kSize {256};
  generateAndWriteData(kSize);

  size_t total_read {};
  EXPECT_EQ(read_to_eof(readfd(), &total_read), 0);
  EXPECT_EQ(total_read, kSize);
}

TEST_F(ReadTest, ReadSmallThenToEOF) {
  constexpr auto kWriteSize {256};
  constexpr auto kReadSize {64};
  generateAndWriteData(kWriteSize);

  auto read_data = readData(kReadSize);
  size_t to_eof {};
  EXPECT_EQ(read_to_eof(readfd(), &to_eof), 0);
  EXPECT_EQ(to_eof, kWriteSize - kReadSize);
}

TEST_F(ReadTest, ReadAllThenToEOF) {
  constexpr auto kSize {256};
  generateAndWriteData(kSize);

  auto read_data = readData(kSize);
  size_t to_eof {};
  EXPECT_EQ(read_to_eof(readfd(), &to_eof), 0);
  EXPECT_EQ(to_eof, 0);
}

TEST_F(ReadTest, ReadBadFdToEOF) {
  size_t to_eof {};
  EXPECT_EQ(read_to_eof(-1, &to_eof), -1);
}

TEST_F(ReadTest, OverflowReadToEOF) {
  int devzero = open("/dev/zero", O_RDONLY);
  ASSERT_NE(devzero, -1);
  SCOPE_EXIT {
    close(devzero);
  };

  size_t to_eof {};
  ASSERT_EQ(read_to_eof(devzero, &to_eof), -1);
}

TEST_F(ReadTest, ReadAndDrainUndersizedBuffer) {
  constexpr auto kWriteSize {256};
  constexpr auto kReadSize {kWriteSize / 2};
  auto data = generateAndWriteData(kWriteSize);

  auto read_data = readData(kReadSize, readfd(), read_and_drain);
  EXPECT_TRUE(std::equal(data.begin(), data.begin() + kReadSize, read_data.first.begin()));
  ASSERT_EQ(read_data.first.size(), kWriteSize);
  ASSERT_EQ(read_data.second, 0);
}

TEST_F(ReadTest, ReadAndDrainExact) {
  constexpr auto kSize {256};
  auto data = generateAndWriteData(kSize);

  auto read_data = readData(kSize, readfd(), read_and_drain);
  ASSERT_EQ(read_data.first, data);
  ASSERT_EQ(read_data.second, 0);
}

TEST_F(ReadTest, ReadAndDrainOversizedBuffer) {
  constexpr auto kWriteSize {256};
  constexpr auto kReadSize {kWriteSize * 2};
  auto data = generateAndWriteData(kWriteSize);

  auto read_data = readData(kReadSize, readfd(), read_and_drain);
  ASSERT_EQ(read_data.first, data);
  ASSERT_EQ(read_data.second, 0);
}





class AtomicReadTest : public ReadTest {
public:
  AtomicReadTest() : file_(std::fopen("/data/data/fbandroid.native.procmaps.unit_test/temp", "w+")) {
    if (!file_) {
      throw std::system_error(errno, std::generic_category());
    }
  }

  ~AtomicReadTest() {
    fclose(file_);
  }

  void performTest(size_t const actual_size, size_t const guess_size) {
    auto data = generateAndWriteData(actual_size, fileno(file_));
    ASSERT_EQ(std::ferror(file_), 0);
    std::rewind(file_);

    void* read_data {};
    size_t data_size {};
    ASSERT_EQ(atomic_read_fd(fileno(file_), guess_size, &read_data, &data_size), 0);
    SCOPE_EXIT {
      xfree(read_data);
    };

    EXPECT_EQ(data_size, data.size());
    for (auto i {0}; i < actual_size; i++) {
      EXPECT_EQ(data[i], reinterpret_cast<uint8_t*>(read_data)[i]);
    }

    // ensure that atomic_read_fd inserted a null terminator for treating data like a C string
    if (actual_size < SIZE_MAX) {
      EXPECT_EQ(reinterpret_cast<uint8_t*>(read_data)[actual_size], 0);
    }
  }

private:
  FILE* const file_;
};

TEST_F(AtomicReadTest, AtomicReadOversizeGuess) {
  performTest(6000, 16 * 1024);
}

TEST_F(AtomicReadTest, AtomicReadExactGuess) {
  performTest(6000, 6000);
}

TEST_F(AtomicReadTest, AtomicReadUndersizeGuess) {
  performTest(6000, 16);
}

TEST_F(AtomicReadTest, AtomicReadZeroGuess) {
  performTest(6000, 0);
}

TEST_F(AtomicReadTest, EnsureNullTerminatorEvenAcrossPageBoundary) {
  performTest(4 * PAGE_SIZE, PAGE_SIZE);
}

TEST_F(AtomicReadTest, FailIfNotSeekable) {
  constexpr auto kSize {6000};
  generateAndWriteData(kSize, writefd());

  void* read_data {};
  size_t data_size {};
  EXPECT_EQ(atomic_read_fd(readfd(), kSize, &read_data, &data_size), -1);
}






constexpr char const kGoalPath[] {"/proc/123456/maps"};
constexpr size_t kGoalPathLen { sizeof(kGoalPath) };
constexpr pid_t kPid {123456};

TEST(BuildProcPath, EnoughSpace) {
  char buf[256];
  auto len = build_proc_path(buf, 256, kPid);
  EXPECT_STREQ(buf, kGoalPath);
  EXPECT_EQ(len, strlen(buf));
}

TEST(BuildProcPath, ExactSpace) {
  char buf[kGoalPathLen];
  auto len = build_proc_path(buf, kGoalPathLen, kPid);
  EXPECT_STREQ(buf, kGoalPath);
  EXPECT_EQ(len, strlen(buf));
}

TEST(BuildProcPath, TooShort) {
  char buf[8];
  auto len = build_proc_path(buf, 8, kPid);
  EXPECT_EQ(len, 0);
}





constexpr char kString[] { "hello, world!" };

TEST(FindFirst, SingleMatch) {
  EXPECT_EQ(find_first(const_cast<char*>(kString), 'o'), strchr(kString, 'o'));
}

TEST(FindFirst, NoMatch) {
  EXPECT_EQ(find_first(const_cast<char*>(kString), 'x'), nullptr);
}

TEST(FindFirst, MultipleMatch) {
  char* ff = find_first(const_cast<char*>(kString), 'l');
  const char* sc = strchr(kString, 'l');
  EXPECT_EQ(ff, sc); // heLlo, world!

  ff = find_first(++ff, 'l');
  sc = strchr(++sc, 'l');
  EXPECT_EQ(ff, sc); // helLo, world!

  ff = find_first(++ff, 'l');
  sc = strchr(++sc, 'l');
  EXPECT_EQ(ff, sc); // hello, worLd!

  ff = find_first(++ff, 'l');
  sc = strchr(++sc, 'l');
  EXPECT_EQ(ff, sc);
  EXPECT_EQ(ff, nullptr);
}

TEST(CountChar, Single) {
  EXPECT_EQ(count_char(const_cast<char*>(kString), 'h'), 1);
}

TEST(CountChar, Multiple) {
  EXPECT_EQ(count_char(const_cast<char*>(kString), 'l'), 3);
}

TEST(CountChar, None) {
  EXPECT_EQ(count_char(const_cast<char*>(kString), 'x'), 0);
}




constexpr char kHexWithDelim[] { "FaCeB00c " };
TEST(ParseHex, MixedCaseWithDelim) {
  unsigned long long out {};
  auto end = parse_hex(const_cast<char*>(kHexWithDelim), ' ', &out);
  EXPECT_EQ(out, 0xfaceb00c);
  EXPECT_EQ(end, &kHexWithDelim[8]);
}
