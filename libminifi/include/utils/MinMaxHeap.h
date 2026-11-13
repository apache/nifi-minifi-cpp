// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <functional>
#include <set>
#include <cmath>
#include <utility>

#include "minifi-cpp/utils/gsl.h"

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
