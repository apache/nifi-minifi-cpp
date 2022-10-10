/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
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

#include <vector>
#include <mutex>

namespace org::apache::nifi::minifi::utils {

template<typename T>
concept Summable = requires(T x) { x + x; };  // NOLINT(readability/braces)

template<typename T>
concept DividableByInteger = requires(T x, uint32_t divisor) { x / divisor; };  // NOLINT(readability/braces)

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
class Averager {
 public:
  explicit Averager(uint32_t sample_size) : SAMPLE_SIZE_(sample_size) {
    values_.reserve(SAMPLE_SIZE_);
  }

  ValueType getAverage() const;
  ValueType getLastValue() const;
  void addValue(ValueType runtime);

 private:
  const uint32_t SAMPLE_SIZE_;
  mutable std::mutex average_value_mutex_;
  uint32_t next_average_index_ = 0;
  std::vector<ValueType> values_;
};

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
ValueType Averager<ValueType>::getAverage() const {
  if (values_.empty()) {
    return {};
  }
  ValueType sum{};
  std::lock_guard<std::mutex> lock(average_value_mutex_);
  for (const auto& value : values_) {
    sum += value;
  }
  return sum / values_.size();
}

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
void Averager<ValueType>::addValue(ValueType runtime) {
  std::lock_guard<std::mutex> lock(average_value_mutex_);
  if (values_.size() < SAMPLE_SIZE_) {
    values_.push_back(runtime);
  } else {
    if (next_average_index_ >= values_.size()) {
      next_average_index_ = 0;
    }
    values_[next_average_index_] = runtime;
    ++next_average_index_;
  }
}

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
ValueType Averager<ValueType>::getLastValue() const {
  std::lock_guard<std::mutex> lock(average_value_mutex_);
  if (values_.empty()) {
    return {};
  } else if (values_.size() < SAMPLE_SIZE_) {
    return values_[values_.size() - 1];
  } else {
    return values_[next_average_index_ - 1];
  }
}

}  // namespace org::apache::nifi::minifi::utils
