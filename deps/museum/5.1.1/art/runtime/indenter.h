/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_INDENTER_H_
#define ART_RUNTIME_INDENTER_H_

#include "base/logging.h"
#include "base/macros.h"
#include <streambuf>

const char kIndentChar =' ';
const size_t kIndentBy1Count = 2;

class Indenter : public std::streambuf {
 public:
  Indenter(std::streambuf* out, char text, size_t count)
      : indent_next_(true), out_sbuf_(out), text_(text), count_(count) {}

 private:
  int_type overflow(int_type c) {
    if (UNLIKELY(c == std::char_traits<char>::eof())) {
      out_sbuf_->pubsync();
      return c;
    }
    if (indent_next_) {
      for (size_t i = 0; i < count_; ++i) {
        int_type r = out_sbuf_->sputc(text_);
        if (UNLIKELY(r != text_)) {
          out_sbuf_->pubsync();
          r = out_sbuf_->sputc(text_);
          CHECK_EQ(r, text_) << "Error writing to buffer. Disk full?";
        }
      }
    }
    indent_next_ = (c == '\n');
    int_type r = out_sbuf_->sputc(c);
    if (UNLIKELY(r != c)) {
      out_sbuf_->pubsync();
      r = out_sbuf_->sputc(c);
      CHECK_EQ(r, c) << "Error writing to buffer. Disk full?";
    }
    return r;
  }

  int sync() {
    return out_sbuf_->pubsync();
  }

  bool indent_next_;

  // Buffer to write output to.
  std::streambuf* const out_sbuf_;

  // Text output as indent.
  const char text_;

  // Number of times text is output.
  const size_t count_;

  DISALLOW_COPY_AND_ASSIGN(Indenter);
};

#endif  // ART_RUNTIME_INDENTER_H_
