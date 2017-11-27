/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_TEST_H_
#define ART_RUNTIME_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_TEST_H_

#include <errno.h>
#include <memory>
#include <string>

#include "common_runtime_test.h"

namespace unix_file {

class RandomAccessFileTest : public testing::Test {
 protected:
  virtual ~RandomAccessFileTest() {
  }

  // Override this to return an instance of the subclass under test that's
  // backed by a temporary file.
  virtual RandomAccessFile* MakeTestFile() = 0;

  virtual void SetUp() {
    art::CommonRuntimeTest::SetUpAndroidData(android_data_);
  }

  virtual void TearDown() {
    art::CommonRuntimeTest::TearDownAndroidData(android_data_, true);
  }

  std::string GetTmpPath(const std::string& name) {
    std::string path;
    path = android_data_;
    path += "/";
    path += name;
    return path;
  }

  // TODO(enh): ReadString (and WriteString) might be generally useful.
  static bool ReadString(RandomAccessFile* f, std::string* s) {
    s->clear();
    char buf[256];
    int64_t n = 0;
    int64_t offset = 0;
    while ((n = f->Read(buf, sizeof(buf), offset)) > 0) {
      s->append(buf, n);
      offset += n;
    }
    return n != -1;
  }

  void TestRead() {
    char buf[256];
    std::unique_ptr<RandomAccessFile> file(MakeTestFile());

    // Reading from the start of an empty file gets you zero bytes, however many
    // you ask for.
    ASSERT_EQ(0, file->Read(buf, 0, 0));
    ASSERT_EQ(0, file->Read(buf, 123, 0));

    const std::string content("hello");
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->Write(content.data(), content.size(), 0)));

    TestReadContent(content, file.get());
  }

  void TestReadContent(const std::string& content, RandomAccessFile* file) {
    const int buf_size = content.size() + 10;
    std::unique_ptr<char> buf(new char[buf_size]);
    // Can't read from a negative offset.
    ASSERT_EQ(-EINVAL, file->Read(buf.get(), 0, -123));

    // Reading too much gets us just what's in the file.
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->Read(buf.get(), buf_size, 0)));
    ASSERT_EQ(std::string(buf.get(), content.size()), content);

    // We only get as much as we ask for.
    const size_t short_request = 2;
    ASSERT_LT(short_request, content.size());
    ASSERT_EQ(short_request, static_cast<uint64_t>(file->Read(buf.get(), short_request, 0)));
    ASSERT_EQ(std::string(buf.get(), short_request),
              content.substr(0, short_request));

    // We don't have to start at the beginning.
    const int non_zero_offset = 2;
    ASSERT_GT(non_zero_offset, 0);
    ASSERT_EQ(short_request, static_cast<uint64_t>(file->Read(buf.get(), short_request,
                                                              non_zero_offset)));
    ASSERT_EQ(std::string(buf.get(), short_request),
              content.substr(non_zero_offset, short_request));

    // Reading past the end gets us nothing.
    ASSERT_EQ(0, file->Read(buf.get(), buf_size, file->GetLength()));
    ASSERT_EQ(0, file->Read(buf.get(), buf_size, file->GetLength() + 1));
  }

  void TestSetLength() {
    const std::string content("hello");
    std::unique_ptr<RandomAccessFile> file(MakeTestFile());
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->Write(content.data(), content.size(), 0)));
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->GetLength()));

    // Can't give a file a negative length.
    ASSERT_EQ(-EINVAL, file->SetLength(-123));

    // Can truncate the file.
    int64_t new_length = 2;
    ASSERT_EQ(0, file->SetLength(new_length));
    ASSERT_EQ(new_length, file->GetLength());
    std::string new_content;
    ASSERT_TRUE(ReadString(file.get(), &new_content));
    ASSERT_EQ(content.substr(0, 2), new_content);

    // Expanding the file appends zero bytes.
    new_length = file->GetLength() + 1;
    ASSERT_EQ(0, file->SetLength(new_length));
    ASSERT_EQ(new_length, file->GetLength());
    ASSERT_TRUE(ReadString(file.get(), &new_content));
    ASSERT_EQ('\0', new_content[new_length - 1]);
  }

  void TestWrite() {
    const std::string content("hello");
    std::unique_ptr<RandomAccessFile> file(MakeTestFile());

    // Can't write to a negative offset.
    ASSERT_EQ(-EINVAL, file->Write(content.data(), 0, -123));

    // Writing zero bytes of data is a no-op.
    ASSERT_EQ(0, file->Write(content.data(), 0, 0));
    ASSERT_EQ(0, file->GetLength());

    // We can write data.
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->Write(content.data(), content.size(), 0)));
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->GetLength()));
    std::string new_content;
    ASSERT_TRUE(ReadString(file.get(), &new_content));
    ASSERT_EQ(new_content, content);

    // We can read it back.
    char buf[256];
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->Read(buf, sizeof(buf), 0)));
    ASSERT_EQ(std::string(buf, content.size()), content);

    // We can append data past the end.
    ASSERT_EQ(content.size(), static_cast<uint64_t>(file->Write(content.data(), content.size(),
                                                                file->GetLength() + 1)));
    int64_t new_length = 2*content.size() + 1;
    ASSERT_EQ(file->GetLength(), new_length);
    ASSERT_TRUE(ReadString(file.get(), &new_content));
    ASSERT_EQ(std::string("hello\0hello", new_length), new_content);
  }

 protected:
  std::string android_data_;
};

}  // namespace unix_file

#endif  // ART_RUNTIME_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_TEST_H_
