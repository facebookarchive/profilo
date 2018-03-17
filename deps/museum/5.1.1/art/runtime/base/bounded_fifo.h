/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_BOUNDED_FIFO_H_
#define ART_RUNTIME_BASE_BOUNDED_FIFO_H_

namespace art {

// A bounded fifo is a fifo which has a bounded size. The power of two version uses a bit mask to
// avoid needing to deal with wrapping integers around or using a modulo operation.
template <typename T, const size_t MaxSize>
class BoundedFifoPowerOfTwo {
 public:
  BoundedFifoPowerOfTwo() {
    // TODO: Do this with a compile time check.
    CHECK(IsPowerOfTwo(MaxSize));
    clear();
  }

  void clear() {
    back_index_ = 0;
    size_ = 0;
  }

  bool empty() const {
    return size() == 0;
  }

  size_t size() const {
    return size_;
  }

  void push_back(const T& value) {
    ++size_;
    DCHECK_LE(size_, MaxSize);
    // Relies on integer overflow behavior.
    data_[back_index_++ & mask_] = value;
  }

  const T& front() const {
    DCHECK_GT(size_, 0U);
    return data_[(back_index_ - size_) & mask_];
  }

  void pop_front() {
    DCHECK_GT(size_, 0U);
    --size_;
  }

 private:
  static const size_t mask_ = MaxSize - 1;
  size_t back_index_, size_;
  T data_[MaxSize];
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_BOUNDED_FIFO_H_
