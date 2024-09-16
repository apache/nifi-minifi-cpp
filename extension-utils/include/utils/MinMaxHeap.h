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

#include <functional>
#include <set>
#include <cmath>
#include <utility>

#include "utils/gsl.h"

struct MinMaxHeapTestAccessor;

namespace org::apache::nifi::minifi::utils {

template<typename T, typename Comparator = std::less<T>>
class MinMaxHeap {
 public:
  void clear() {
    data_.clear();
  }

  const T& min() const {
    return *data_.begin();
  }

  const T& max() const {
    return *data_.rbegin();
  }

  size_t size() const {
    return data_.size();
  }

  bool empty() const {
    return data_.empty();
  }

  void push(T item) {
    data_.insert(std::move(item));
  }

  T popMin() {
    auto it = data_.begin();
    T min = std::move(*it);
    data_.erase(it);
    return min;
  }

  T popMax() {
    auto it = std::prev(data_.end());
    T max = std::move(*it);
    data_.erase(it);
    return max;
  }

 private:
  std::multiset<T, Comparator> data_;
};

}  // namespace org::apache::nifi::minifi::utils
