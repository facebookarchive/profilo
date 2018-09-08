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

#include <yarn/detail/BufferParser.h>

namespace facebook {
namespace yarn {
namespace detail {
namespace parser {

// Returns the number of bytes read
size_t parseEvent(
    void* data,
    size_t offset,
    const Event& bufferEvent,
    IdEventMap& idEventMap,
    RecordListener* listener);
void notifyMmap(void* data, RecordListener* listener);
void notifyForkEnter(void* data, RecordListener* listener);
void notifyForkExit(void* data, RecordListener* listener);
void notifyLost(void* data, RecordListener* listener);
void notifySample(
    void* data,
    size_t size,
    uint64_t sample_type,
    uint64_t read_format,
    const IdEventMap& idEventMap,
    RecordListener* listener);

void parseBuffer(
    const Event& bufferEvent,
    IdEventMap& idEventMap,
    RecordListener* listener) {
  if (bufferEvent.buffer() == nullptr) {
    throw std::invalid_argument("Event must be mapped in order to be parsed");
  }

  void* buffer = bufferEvent.buffer();
  perf_event_mmap_page* header = (perf_event_mmap_page*)buffer;

  /*printf("header version: %.8x\n", header->version);
  printf("  head: %.16llx\n", header->data_head);
  printf("  tail: %.16llx\n", header->data_tail);
  printf("  lock: %.8x\n", header->lock);
  printf("  enabled: %.16llx\n", header->time_enabled);
  printf("  running: %.16llx\n", header->time_running);*/
  printf(
      "buffer {enabled: %llu running: %llu}\n",
      header->time_enabled,
      header->time_running);

  uint8_t* data = ((uint8_t*)buffer) + PAGE_SIZE;
  size_t last_read = header->data_tail;

  size_t buffer_data_size = bufferEvent.bufferSize() - PAGE_SIZE;

  while (last_read < header->data_head) {
    // data_head and data_tail (last_read) are not restricted to within the
    // buffer boundaries. Wrap explicitly to find the offset within the buffer.
    size_t offset = (last_read % buffer_data_size);
    last_read += parseEvent(data, offset, bufferEvent, idEventMap, listener);
  }
  header->data_tail = last_read;
}

size_t parseEvent(
    void* data,
    size_t offset,
    const Event& bufferEvent,
    IdEventMap& idEventMap,
    RecordListener* listener) {
  uint8_t split_buffer[128]; // buffer for reassembling split records

  size_t buffer_data_size = bufferEvent.bufferSize() - PAGE_SIZE;
  if (offset + sizeof(perf_event_header) > buffer_data_size) {
    throw std::runtime_error("Unhandled: split perf_event_header");
  }

  perf_event_header* evt_header = (perf_event_header*)((uint8_t*)data + offset);
  uint8_t* data_bytes = ((uint8_t*)evt_header) + sizeof(perf_event_header);

  // Note: evt_header->size includes the size of the header itself
  if (offset + evt_header->size > buffer_data_size) {
    // Split read, copy to buffer and present a contiguous view to the
    // listeners/etc.
    if (evt_header->size > sizeof(split_buffer)) {
      throw std::runtime_error("Split event is bigger than our buffer");
    }
    size_t bytes_to_end =
        buffer_data_size - (offset + sizeof(perf_event_header));
    std::memcpy(split_buffer, data_bytes, bytes_to_end);
    std::memcpy(
        split_buffer + bytes_to_end, data, evt_header->size - bytes_to_end);

    data_bytes = split_buffer;
  }

  int type = evt_header->type;
  switch (type) {
    case PERF_RECORD_SAMPLE: {
      auto attr = bufferEvent.attr();
      notifySample(
          data_bytes,
          evt_header->size,
          attr.sample_type,
          attr.read_format,
          idEventMap,
          listener);
      break;
    }
    case PERF_RECORD_MMAP:
      notifyMmap(data_bytes, listener);
      break;
    case PERF_RECORD_FORK:
      notifyForkEnter(data_bytes, listener);
      break;
    case PERF_RECORD_EXIT:
      notifyForkExit(data_bytes, listener);
      break;
    case PERF_RECORD_LOST:
      notifyLost(data_bytes, listener);
      break;
    case PERF_RECORD_COMM:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
      // Currently unhandled
      break;
    default:
      throw std::runtime_error("Unhandled event type");
  }

  return evt_header->size;
}

void notifySample(
    void* data,
    size_t size,
    uint64_t sample_type,
    uint64_t read_format,
    const IdEventMap& idEventMap,
    RecordListener* listener) {
  if (listener == nullptr) {
    return;
  }
  RecordSample rec(data, size, sample_type, read_format);

  // Need groupLeaderId() because inheritance may give us id()s which we never
  // set up explicitly. We're
  auto type = idEventMap.at(rec.groupLeaderId()).type();
  listener->onSample(type, rec);
}

void notifyMmap(void* data, RecordListener* listener) {
  if (listener == nullptr) {
    return;
  }
  RecordMmap* rec = (RecordMmap*)data;
  listener->onMmap(*rec);
}

void notifyForkEnter(void* data, RecordListener* listener) {
  if (listener == nullptr) {
    return;
  }
  RecordForkExit* rec = (RecordForkExit*)data;
  listener->onForkEnter(*rec);
}

void notifyForkExit(void* data, RecordListener* listener) {
  if (listener == nullptr) {
    return;
  }
  RecordForkExit* rec = (RecordForkExit*)data;
  listener->onForkExit(*rec);
}

void notifyLost(void* data, RecordListener* listener) {
  if (listener == nullptr) {
    return;
  }
  RecordLost* rec = (RecordLost*)data;
  listener->onLost(*rec);
}

} // namespace parser
} // namespace detail
} // namespace yarn
} // namespace facebook
