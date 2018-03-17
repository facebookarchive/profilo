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
#include <ostream>
#include <streambuf>

namespace art {

constexpr char kIndentChar =' ';
constexpr size_t kIndentBy1Count = 2;

class Indenter : public std::streambuf {
 public:
  Indenter(std::streambuf* out, char text, size_t count)
      : indent_next_(true), out_sbuf_(out),
        text_{text, text, text, text, text, text, text, text},  // NOLINT(whitespace/braces)
        count_(count) {}

 private:
  std::streamsize xsputn(const char* s, std::streamsize n) OVERRIDE {
    std::streamsize result = n;  // Aborts on failure.
    const char* eol = static_cast<const char*>(memchr(s, '\n', n));
    while (eol != nullptr) {
      size_t to_write = eol + 1 - s;
      Write(s, to_write);
      s += to_write;
      n -= to_write;
      indent_next_ = true;
      eol = static_cast<const char*>(memchr(s, '\n', n));
    }
    if (n != 0u) {
      Write(s, n);
    }
    return result;
  }

  int_type overflow(int_type c) OVERRIDE {
    if (UNLIKELY(c == std::char_traits<char>::eof())) {
      out_sbuf_->pubsync();
      return c;
    }
    char data[1] = { static_cast<char>(c) };
    Write(data, 1u);
    indent_next_ = (c == '\n');
    return c;
  }

  int sync() {
    return out_sbuf_->pubsync();
  }

  void Write(const char* s, std::streamsize n) {
    if (indent_next_) {
      size_t remaining = count_;
      while (remaining != 0u) {
        size_t to_write = std::min(remaining, sizeof(text_));
        RawWrite(text_, to_write);
        remaining -= to_write;
      }
      indent_next_ = false;
    }
    RawWrite(s, n);
  }

  void RawWrite(const char* s, std::streamsize n) {
    size_t written = out_sbuf_->sputn(s, n);
    s += written;
    n -= written;
    while (n != 0u) {
      out_sbuf_->pubsync();
      written = out_sbuf_->sputn(s, n);
      CHECK_NE(written, 0u) << "Error writing to buffer. Disk full?";
      s += written;
      n -= written;
    }
  }

  bool indent_next_;

  // Buffer to write output to.
  std::streambuf* const out_sbuf_;

  // Text output as indent.
  const char text_[8];

  // Number of times text is output.
  size_t count_;

  friend class VariableIndentationOutputStream;

  DISALLOW_COPY_AND_ASSIGN(Indenter);
};

class VariableIndentationOutputStream {
 public:
  explicit VariableIndentationOutputStream(std::ostream* os, char text = kIndentChar)
      : indenter_(os->rdbuf(), text, 0u),
        indented_os_(&indenter_) {
  }

  std::ostream& Stream() {
    return indented_os_;
  }

  void IncreaseIndentation(size_t adjustment) {
    indenter_.count_ += adjustment;
  }

  void DecreaseIndentation(size_t adjustment) {
    DCHECK_GE(indenter_.count_, adjustment);
    indenter_.count_ -= adjustment;
  }

 private:
  Indenter indenter_;
  std::ostream indented_os_;

  DISALLOW_COPY_AND_ASSIGN(VariableIndentationOutputStream);
};

class ScopedIndentation {
 public:
  explicit ScopedIndentation(VariableIndentationOutputStream* vios,
                             size_t adjustment = kIndentBy1Count)
      : vios_(vios),
        adjustment_(adjustment) {
    vios_->IncreaseIndentation(adjustment_);
  }

  ~ScopedIndentation() {
    vios_->DecreaseIndentation(adjustment_);
  }

 private:
  VariableIndentationOutputStream* const vios_;
  const size_t adjustment_;

  DISALLOW_COPY_AND_ASSIGN(ScopedIndentation);
};

}  // namespace art

#endif  // ART_RUNTIME_INDENTER_H_
