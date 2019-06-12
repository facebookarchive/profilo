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

#include "TestSequencer.h"
#include <cassert>
#include <memory>

namespace facebook {
namespace profilo {
namespace test {

TestSequencer::TestSequencer(int initial, int max_turns)
    : state_(initial),
      max_states_(max_turns),
      semaphores_(std::make_unique<sem_t[]>(max_turns)) {
  for (int i = 0; i < max_turns; ++i) {
    sem_init(&semaphores_[i], 0, 0);
  }
}

TestSequencer::~TestSequencer() {
  for (int i = 0; i < max_states_; ++i) {
    sem_destroy(&semaphores_[i]);
  }
}

void TestSequencer::waitFor(int turn) const {
  while (state_ < turn) {
    sem_wait(&semaphores_[turn - 1]);
  }
}

void TestSequencer::advance() {
  if (state_ < max_states_) {
    // Semaphores are 0-indexed - turn X has its
    // waiters on semaphore X-1.
    sem_post(&semaphores_[state_++]);
  }
}

void TestSequencer::advance(int expectedTurn) {
  if (state_ < max_states_) {
    // Semaphores are 0-indexed - turn X has its
    // waiters on semaphore X-1.
    sem_post(&semaphores_[state_++]);
  }
  assert(state_ == expectedTurn);
}

void TestSequencer::waitAndAdvance(int turn) {
  waitFor(turn);
  advance();
}

void TestSequencer::waitAndAdvance(int turn, int expectedTurn) {
  waitFor(turn);
  advance(expectedTurn);
}

int TestSequencer::turn() const {
  return state_;
}

} // namespace test
} // namespace profilo
} // namespace facebook
