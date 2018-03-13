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

#ifndef ART_RUNTIME_BASE_HISTOGRAM_INL_H_
#define ART_RUNTIME_BASE_HISTOGRAM_INL_H_

#include "histogram.h"

#include "utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>

namespace art {

template <class Value> inline void Histogram<Value>::AddValue(Value value) {
  CHECK_GE(value, static_cast<Value>(0));
  if (value >= max_) {
    Value new_max = ((value + 1) / bucket_width_ + 1) * bucket_width_;
    DCHECK_GT(new_max, max_);
    GrowBuckets(new_max);
  }

  BucketiseValue(value);
}

template <class Value> inline Histogram<Value>::Histogram(const char* name)
    : kAdjust(0),
      kInitialBucketCount(0),
      name_(name),
      max_buckets_(0) {
}

template <class Value>
inline Histogram<Value>::Histogram(const char* name, Value initial_bucket_width,
                                   size_t max_buckets)
    : kAdjust(1000),
      kInitialBucketCount(8),
      name_(name),
      max_buckets_(max_buckets),
      bucket_width_(initial_bucket_width) {
  Reset();
}

template <class Value>
inline void Histogram<Value>::GrowBuckets(Value new_max) {
  while (max_ < new_max) {
    // If we have reached the maximum number of buckets, merge buckets together.
    if (frequency_.size() >= max_buckets_) {
      CHECK(IsAligned<2>(frequency_.size()));
      // We double the width of each bucket to reduce the number of buckets by a factor of 2.
      bucket_width_ *= 2;
      const size_t limit = frequency_.size() / 2;
      // Merge the frequencies by adding each adjacent two together.
      for (size_t i = 0; i < limit; ++i) {
        frequency_[i] = frequency_[i * 2] + frequency_[i * 2 + 1];
      }
      // Remove frequencies in the second half of the array which were added to the first half.
      while (frequency_.size() > limit) {
        frequency_.pop_back();
      }
    }
    max_ += bucket_width_;
    frequency_.push_back(0);
  }
}

template <class Value> inline size_t Histogram<Value>::FindBucket(Value val) const {
  // Since this is only a linear histogram, bucket index can be found simply with
  // dividing the value by the bucket width.
  DCHECK_GE(val, min_);
  DCHECK_LE(val, max_);
  const size_t bucket_idx = static_cast<size_t>((val - min_) / bucket_width_);
  DCHECK_GE(bucket_idx, 0ul);
  DCHECK_LE(bucket_idx, GetBucketCount());
  return bucket_idx;
}

template <class Value>
inline void Histogram<Value>::BucketiseValue(Value val) {
  CHECK_LT(val, max_);
  sum_ += val;
  sum_of_squares_ += val * val;
  ++sample_size_;
  ++frequency_[FindBucket(val)];
  max_value_added_ = std::max(val, max_value_added_);
  min_value_added_ = std::min(val, min_value_added_);
}

template <class Value> inline void Histogram<Value>::Initialize() {
  for (size_t idx = 0; idx < kInitialBucketCount; idx++) {
    frequency_.push_back(0);
  }
  // Cumulative frequency and ranges has a length of 1 over frequency.
  max_ = bucket_width_ * GetBucketCount();
}

template <class Value> inline size_t Histogram<Value>::GetBucketCount() const {
  return frequency_.size();
}

template <class Value> inline void Histogram<Value>::Reset() {
  sum_of_squares_ = 0;
  sample_size_ = 0;
  min_ = 0;
  sum_ = 0;
  min_value_added_ = std::numeric_limits<Value>::max();
  max_value_added_ = std::numeric_limits<Value>::min();
  frequency_.clear();
  Initialize();
}

template <class Value> inline Value Histogram<Value>::GetRange(size_t bucket_idx) const {
  DCHECK_LE(bucket_idx, GetBucketCount());
  return min_ + bucket_idx * bucket_width_;
}

template <class Value> inline double Histogram<Value>::Mean() const {
  DCHECK_GT(sample_size_, 0ull);
  return static_cast<double>(sum_) / static_cast<double>(sample_size_);
}

template <class Value> inline double Histogram<Value>::Variance() const {
  DCHECK_GT(sample_size_, 0ull);
  // Using algorithms for calculating variance over a population:
  // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
  Value sum_squared = sum_ * sum_;
  double sum_squared_by_n_squared =
      static_cast<double>(sum_squared) /
      static_cast<double>(sample_size_ * sample_size_);
  double sum_of_squares_by_n =
      static_cast<double>(sum_of_squares_) / static_cast<double>(sample_size_);
  return sum_of_squares_by_n - sum_squared_by_n_squared;
}

template <class Value>
inline void Histogram<Value>::PrintBins(std::ostream& os, const CumulativeData& data) const {
  DCHECK_GT(sample_size_, 0ull);
  for (size_t bin_idx = 0; bin_idx < data.freq_.size(); ++bin_idx) {
    if (bin_idx > 0 && data.perc_[bin_idx] == data.perc_[bin_idx - 1]) {
      bin_idx++;
      continue;
    }
    os << GetRange(bin_idx) << ": " << data.freq_[bin_idx] << "\t"
       << data.perc_[bin_idx] * 100.0 << "%\n";
  }
}

template <class Value>
inline void Histogram<Value>::PrintConfidenceIntervals(std::ostream &os, double interval,
                                                       const CumulativeData& data) const {
  static constexpr size_t kFractionalDigits = 3;
  DCHECK_GT(interval, 0);
  DCHECK_LT(interval, 1.0);
  const double per_0 = (1.0 - interval) / 2.0;
  const double per_1 = per_0 + interval;
  const TimeUnit unit = GetAppropriateTimeUnit(Mean() * kAdjust);
  os << Name() << ":\tSum: " << PrettyDuration(Sum() * kAdjust) << " "
     << (interval * 100) << "% C.I. " << FormatDuration(Percentile(per_0, data) * kAdjust, unit,
                                                        kFractionalDigits)
     << "-" << FormatDuration(Percentile(per_1, data) * kAdjust, unit, kFractionalDigits) << " "
     << "Avg: " << FormatDuration(Mean() * kAdjust, unit, kFractionalDigits) << " Max: "
     << FormatDuration(Max() * kAdjust, unit, kFractionalDigits) << "\n";
}

template <class Value>
inline void Histogram<Value>::CreateHistogram(CumulativeData* out_data) const {
  DCHECK_GT(sample_size_, 0ull);
  out_data->freq_.clear();
  out_data->perc_.clear();
  uint64_t accumulated = 0;
  out_data->freq_.push_back(accumulated);
  out_data->perc_.push_back(0.0);
  for (size_t idx = 0; idx < frequency_.size(); idx++) {
    accumulated += frequency_[idx];
    out_data->freq_.push_back(accumulated);
    out_data->perc_.push_back(static_cast<double>(accumulated) / static_cast<double>(sample_size_));
  }
  DCHECK_EQ(out_data->freq_.back(), sample_size_);
  DCHECK_LE(std::abs(out_data->perc_.back() - 1.0), 0.001);
}

template <class Value>
inline double Histogram<Value>::Percentile(double per, const CumulativeData& data) const {
  DCHECK_GT(data.perc_.size(), 0ull);
  size_t upper_idx = 0, lower_idx = 0;
  for (size_t idx = 0; idx < data.perc_.size(); idx++) {
    if (per <= data.perc_[idx]) {
      upper_idx = idx;
      break;
    }

    if (per >= data.perc_[idx] && idx != 0 && data.perc_[idx] != data.perc_[idx - 1]) {
      lower_idx = idx;
    }
  }

  const double lower_perc = data.perc_[lower_idx];
  const double lower_value = static_cast<double>(GetRange(lower_idx));
  if (per == lower_perc) {
    return lower_value;
  }

  const double upper_perc = data.perc_[upper_idx];
  const double upper_value = static_cast<double>(GetRange(upper_idx));
  if (per == upper_perc) {
    return upper_value;
  }
  DCHECK_GT(upper_perc, lower_perc);

  double value = lower_value + (upper_value - lower_value) *
                               (per - lower_perc) / (upper_perc - lower_perc);

  if (value < min_value_added_) {
    value = min_value_added_;
  } else if (value > max_value_added_) {
    value = max_value_added_;
  }

  return value;
}

}  // namespace art
#endif  // ART_RUNTIME_BASE_HISTOGRAM_INL_H_

