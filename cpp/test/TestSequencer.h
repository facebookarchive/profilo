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

#pragma once

#include <inttypes.h>
#include <semaphore.h>
#include <atomic>
#include <memory>

namespace facebook {
namespace profilo {
namespace test {

// TestSequencer a turn-based barrier.
// You can either
//  1) wait for a specific turn number or
//  2) increment the turn number.
//
// Coupled with int enums, this makes for fairly easy declaration and sequencing
// of points across threads in a test.
class TestSequencer {
 public:
  explicit TestSequencer(int initial, int max_turns);
  TestSequencer(TestSequencer const&) = delete;
  TestSequencer& operator=(TestSequencer const&) = delete;
  virtual ~TestSequencer();

  void waitFor(int turn) const;

  // Use these versions while developing as they don't dictate what the
  // target state should be. Once the sequence transitions are set in stone,
  // refactor your code to the versions below to document every state
  // transition at the call site.
  //
  // E.g., instead of sequencer.advance(), use sequencer.advance(TARGET_STATE)
  void waitAndAdvance(int turn);
  void advance();

  void waitAndAdvance(int turn, int expectedTurn);
  void advance(int expectedTurn);

  int turn() const;

 private:
  std::atomic_int state_;
  int max_states_;

  mutable std::unique_ptr<sem_t[]> semaphores_;
};

} // namespace test
} // namespace profilo
} // namespace facebook
