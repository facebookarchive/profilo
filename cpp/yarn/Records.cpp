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

#include <yarn/Records.h>

namespace facebook {
namespace yarn {

RecordSample::RecordSample(
    void* data,
    size_t len,
    uint64_t sample_type,
    uint64_t read_format)
    : data_((uint8_t*)data),
      len_(len),
      sample_type_(sample_type),
      read_format_(read_format) {}

uint64_t RecordSample::ip() const {
  return *(reinterpret_cast<uint64_t*>(data_ + offsetForField(PERF_SAMPLE_IP)));
}

uint32_t RecordSample::pid() const {
  return *(
      reinterpret_cast<uint32_t*>(data_ + offsetForField(PERF_SAMPLE_TID)));
}

uint32_t RecordSample::tid() const {
  return *(reinterpret_cast<uint32_t*>(
      data_ + offsetForField(PERF_SAMPLE_TID) + 4 /* skip pid */));
}

uint64_t RecordSample::time() const {
  return *(
      reinterpret_cast<uint64_t*>(data_ + offsetForField(PERF_SAMPLE_TIME)));
}

uint64_t RecordSample::addr() const {
  return *(
      reinterpret_cast<uint64_t*>(data_ + offsetForField(PERF_SAMPLE_ADDR)));
}

uint64_t RecordSample::groupLeaderId() const {
  return *(reinterpret_cast<uint64_t*>(data_ + offsetForField(PERF_SAMPLE_ID)));
}

uint64_t RecordSample::id() const {
  return *(reinterpret_cast<uint64_t*>(
      data_ + offsetForField(PERF_SAMPLE_STREAM_ID)));
}

uint32_t RecordSample::cpu() const {
  return *(
      reinterpret_cast<uint32_t*>(data_ + offsetForField(PERF_SAMPLE_CPU)));
}

uint64_t RecordSample::timeRunning() const {
  return *(reinterpret_cast<uint64_t*>(
      data_ + offsetForField(PERF_FORMAT_TOTAL_TIME_RUNNING)));
}

uint64_t RecordSample::timeEnabled() const {
  return *(reinterpret_cast<uint64_t*>(
      data_ + offsetForField(PERF_FORMAT_TOTAL_TIME_ENABLED)));
}

size_t RecordSample::size() const {
  return len_;
}

size_t RecordSample::offsetForField(uint64_t field) const {
  size_t offset = 0;

  if ((sample_type_ & PERF_SAMPLE_IDENTIFIER) != 0) {
    if (field == PERF_SAMPLE_IDENTIFIER) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type_ & PERF_SAMPLE_IP) != 0) {
    if (field == PERF_SAMPLE_IP) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type_ & PERF_SAMPLE_TID) != 0) {
    if (field == PERF_SAMPLE_TID) {
      return offset;
    }
    offset += 2 * sizeof(uint32_t); // uint32_t tid, pid
  }

  if ((sample_type_ & PERF_SAMPLE_TIME) != 0) {
    if (field == PERF_SAMPLE_TIME) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type_ & PERF_SAMPLE_ADDR) != 0) {
    if (field == PERF_SAMPLE_ADDR) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type_ & PERF_SAMPLE_ID) != 0) {
    if (field == PERF_SAMPLE_ID) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type_ & PERF_SAMPLE_STREAM_ID) != 0) {
    if (field == PERF_SAMPLE_STREAM_ID) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type_ & PERF_SAMPLE_CPU) != 0) {
    if (field == PERF_SAMPLE_CPU) {
      return offset;
    }
    offset += 2 * sizeof(uint32_t); // uint32_t cpu, __res;
  }

  if ((sample_type_ & PERF_SAMPLE_PERIOD) != 0) {
    if (field == PERF_SAMPLE_PERIOD) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  bool field_in_read_format = field == PERF_FORMAT_ID ||
      field == PERF_FORMAT_TOTAL_TIME_RUNNING ||
      field == PERF_FORMAT_TOTAL_TIME_ENABLED;

  if ((sample_type_ & PERF_SAMPLE_READ) != 0) {
    if (field_in_read_format) {
      return offset + offsetForReadFormat(field);
    }

    // Skip the entire read_format struct.
    // Every field in read_format is a u64, +1 because `value` is always
    // present.
    size_t bytes_in_read_format =
        (__builtin_popcountll(read_format_) + 1) * sizeof(uint64_t);
    offset += bytes_in_read_format;
  } else if (field_in_read_format) {
    throw std::logic_error(
        "Attempting to access field in read_format without PERF_SAMPLE_READ in sample_type");
  }

  throw std::invalid_argument("Unknown field for sample_type");
}

size_t RecordSample::offsetForReadFormat(uint64_t field) const {
  size_t offset = sizeof(uint64_t); // skip initial `value` field, no `field`
                                    // value corresponds to it.

  if ((read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) != 0) {
    if (field == PERF_FORMAT_TOTAL_TIME_ENABLED) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) != 0) {
    if (field == PERF_FORMAT_TOTAL_TIME_RUNNING) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((read_format_ & PERF_FORMAT_ID) != 0) {
    if (field == PERF_FORMAT_ID) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }
  throw std::invalid_argument("Unknown field for read_format");
}

} // namespace yarn
} // namespace facebook
