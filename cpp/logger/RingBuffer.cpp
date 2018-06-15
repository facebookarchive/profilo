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

#include <profilo/RingBuffer.h>

#include <fb/log.h>

#include <atomic>
#include <memory>

namespace facebook { namespace profilo {

namespace {

TraceBuffer noop_buffer(1);
std::atomic<TraceBuffer*> buffer(&noop_buffer);

} // namespace anonymous

TraceBuffer& RingBuffer::init(size_t sz) {
  if (buffer.load() != &noop_buffer) {
    // Already initialized
    return get();
  }

  auto expected = &noop_buffer;
  auto new_buffer = new TraceBuffer(sz);

  // We expect the update succeed only once for noop_buffer
  if (!buffer.compare_exchange_strong(expected, new_buffer)) {
      delete new_buffer;
      FBLOGE("Second attempt to init the TraceBuffer");
  }

  return get();
}

TraceBuffer& RingBuffer::get() {
  return *buffer.load();
}

} } // namespace facebook::profilo
