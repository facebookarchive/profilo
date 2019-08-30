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

#include <perfevents/Records.h>
#include <perfevents/Event.h>
#include <perfevents/detail/FileBackedMappingsList.h>

#include <cstring>

namespace facebook {
namespace perfevents {

bool RecordMmap::isAnonymous() const {
  // Purely anonymous entries have //anon as the filename.
  static constexpr char kAnonPath[] = "//anon";

  if (strncmp(filename, kAnonPath, strlen(kAnonPath)) == 0) {
    return true;
  }

  // There are other named entries that are also anonymous.
  // (e.g., [stack:1000])
  if (detail::FileBackedMappingsList::isAnonymous(filename)) {
    return true;
  }

  return false;
}

RecordSample::RecordSample(void* data, size_t len)
    : data_((uint8_t*)data), len_(len) {}

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

namespace {
// Offset calculation routines for arbitrary sample_type and read_format
// values. Used to generate constexpr constants for the sample_type and
// read_format we actually use. Otherwise, it's not at all obvious how you
// compute the offsets or what needs to change if you add something new to
// kSampleType.
//
constexpr static size_t genericOffsetForReadFormat(
    uint64_t read_format,
    uint64_t field) {
  size_t offset = sizeof(uint64_t); // skip initial `value` field, no `field`
                                    // value corresponds to it.

  if ((read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) != 0) {
    if (field == PERF_FORMAT_TOTAL_TIME_ENABLED) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) != 0) {
    if (field == PERF_FORMAT_TOTAL_TIME_RUNNING) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((read_format & PERF_FORMAT_ID) != 0) {
    if (field == PERF_FORMAT_ID) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }
  return offset;
}

constexpr static size_t genericOffsetForField(
    uint64_t sample_type,
    uint64_t read_format,
    uint64_t field) {
  size_t offset = 0;

  if ((sample_type & PERF_SAMPLE_IDENTIFIER) != 0) {
    if (field == PERF_SAMPLE_IDENTIFIER) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type & PERF_SAMPLE_IP) != 0) {
    if (field == PERF_SAMPLE_IP) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type & PERF_SAMPLE_TID) != 0) {
    if (field == PERF_SAMPLE_TID) {
      return offset;
    }
    offset += 2 * sizeof(uint32_t); // uint32_t tid, pid
  }

  if ((sample_type & PERF_SAMPLE_TIME) != 0) {
    if (field == PERF_SAMPLE_TIME) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type & PERF_SAMPLE_ADDR) != 0) {
    if (field == PERF_SAMPLE_ADDR) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type & PERF_SAMPLE_ID) != 0) {
    if (field == PERF_SAMPLE_ID) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type & PERF_SAMPLE_STREAM_ID) != 0) {
    if (field == PERF_SAMPLE_STREAM_ID) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  if ((sample_type & PERF_SAMPLE_CPU) != 0) {
    if (field == PERF_SAMPLE_CPU) {
      return offset;
    }
    offset += 2 * sizeof(uint32_t); // uint32_t cpu, __res;
  }

  if ((sample_type & PERF_SAMPLE_PERIOD) != 0) {
    if (field == PERF_SAMPLE_PERIOD) {
      return offset;
    }
    offset += sizeof(uint64_t);
  }

  bool field_in_read_format = field == PERF_FORMAT_ID ||
      field == PERF_FORMAT_TOTAL_TIME_RUNNING ||
      field == PERF_FORMAT_TOTAL_TIME_ENABLED;

  if ((sample_type & PERF_SAMPLE_READ) != 0) {
    if (field_in_read_format) {
      return offset + genericOffsetForReadFormat(read_format, field);
    }

    // Skip the entire read_format struct.
    // Every field in read_format is a u64, +1 because `value` is always
    // present.
    size_t bytes_in_read_format =
        (__builtin_popcountll(read_format) + 1) * sizeof(uint64_t);
    offset += bytes_in_read_format;
  } else if (field_in_read_format) {
    // If we hit this branch, we will de-constexpr-ify the function anyway.
    throw std::logic_error(
        "Attempting to access field in read_format without PERF_SAMPLE_READ in sample_type");
  }

  return offset;
}

} // namespace

size_t RecordSample::offsetForField(uint64_t field) const {
  static constexpr uint64_t kTidOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_TID);
  static constexpr uint64_t kTimeOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_TIME);
  static constexpr uint64_t kAddrOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_ADDR);
  static constexpr uint64_t kIdOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_ID);
  static constexpr uint64_t kStreamIdOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_STREAM_ID);
  static constexpr uint64_t kCpuOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_CPU);
  static constexpr uint64_t kReadOffset =
      genericOffsetForField(kSampleType, kReadFormat, PERF_SAMPLE_READ);

  switch (field) {
    case PERF_SAMPLE_TID:
      return kTidOffset;
    case PERF_SAMPLE_TIME:
      return kTimeOffset;
    case PERF_SAMPLE_ADDR:
      return kAddrOffset;
    case PERF_SAMPLE_ID:
      return kIdOffset;
    case PERF_SAMPLE_STREAM_ID:
      return kStreamIdOffset;
    case PERF_SAMPLE_CPU:
      return kCpuOffset;
    case PERF_SAMPLE_READ:
      return kReadOffset;
  }
  throw std::invalid_argument("Requested field not in kSampleType");
}

} // namespace perfevents
} // namespace facebook
