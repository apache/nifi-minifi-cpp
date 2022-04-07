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
#include <vector>
#include <cmath>
#include <utility>

#include "utils/gsl.h"

struct MinMaxHeapTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T, typename Comparator = std::less<T>>
class MinMaxHeap {
 public:
  void clear() {
    data_.clear();
  }

  const T& min() const {
    return data_[0];
  }

  const T& max() const {
    return data_[getMaxIndex()];
  }

  size_t size() const {
    return data_.size();
  }

  bool empty() const {
    return data_.empty();
  }

  void push(T item) {
    data_.push_back(std::move(item));
    pushUp(data_.size() - 1);
  }

  T popMin() {
    std::swap(data_[0], data_[data_.size() - 1]);
    T item = std::move(data_.back());
    data_.pop_back();
    pushDown(0);
    return item;
  }

  T popMax() {
    size_t max_index = getMaxIndex();
    std::swap(data_[max_index], data_.back());
    T item = std::move(data_.back());
    data_.pop_back();
    if (max_index < data_.size()) {
      pushDown(max_index);
    }
    return item;
  }

 private:
  size_t getMaxIndex() const {
    if (data_.size() <= 2) {
      return data_.size() - 1;
    }
    if (less_(data_[2], data_[1])) {
      return 1;
    }
    return 2;
  }

  friend struct ::MinMaxHeapTestAccessor;

  static size_t getLevel(size_t index) {
    // more performant solutions are possible
    // investigate if this turns out to be a bottleneck
    size_t level = 0;
    ++index;
    while (index >>= 1) {
      ++level;
    }
    return level;
  }

  static bool isOnMinLevel(size_t index) {
    return getLevel(index) % 2 == 0;
  }

  static size_t getParent(size_t index) {
    return (index - 1) / 2;
  }

  /**
   * WARNING! must only be called when index is on a min-level.
   * Smallest child or grandchild's index or 0 if there are no children.
   * @param index
   * @return index of smallest child, grandchild or 0
   */
  size_t getSmallestChildOrGrandchild(size_t index) const {
    gsl_ExpectsAudit(isOnMinLevel(index));
    const size_t left = 2 * index + 1;
    const size_t right = 2 * index + 2;
    const size_t ll = 2 * left + 1;
    const size_t lr = 2 * left + 2;
    const size_t rl = 2 * right + 1;
    const size_t rr = 2 * right + 2;
    if (rr < data_.size()) {
      // compare only grandchildren
      const size_t l_min = less_(data_[ll], data_[lr]) ? ll : lr;
      const size_t r_min = less_(data_[rl], data_[rr]) ? rl : rr;
      return less_(data_[l_min], data_[r_min]) ? l_min : r_min;
    }
    if (rl < data_.size()) {
      // rr is invalid
      const size_t l_min = less_(data_[ll], data_[lr]) ? ll : lr;
      return less_(data_[l_min], data_[rl]) ? l_min : rl;
    }
    if (lr < data_.size()) {
      // no right grandchildren
      const size_t l_min = less_(data_[ll], data_[lr]) ? ll : lr;
      return less_(data_[right], data_[l_min]) ? right : l_min;
    }
    if (ll < data_.size()) {
      // single left grandchild
      return less_(data_[right], data_[ll]) ? right : ll;
    }
    if (right < data_.size()) {
      // only children
      return less_(data_[left], data_[right]) ? left : right;
    }
    if (left < data_.size()) {
      // single left child
      return left;
    }
    return 0;
  }

  /**
   * WARNING! must only be called when index is on a max-level.
   * Largest child or grandchild's index or 0 if there are no children.
   * @param index
   * @return index of largest child, grandchild or 0
   */
  size_t getLargestChildOrGrandchild(size_t index) const {
    gsl_ExpectsAudit(!isOnMinLevel(index));
    const size_t left = 2 * index + 1;
    const size_t right = 2 * index + 2;
    const size_t ll = 2 * left + 1;
    const size_t lr = 2 * left + 2;
    const size_t rl = 2 * right + 1;
    const size_t rr = 2 * right + 2;
    if (rr < data_.size()) {
      // compare only grandchildren
      const size_t l_max = less_(data_[lr], data_[ll]) ? ll : lr;
      const size_t r_max = less_(data_[rr], data_[rl]) ? rl : rr;
      return less_(data_[r_max], data_[l_max]) ? l_max : r_max;
    }
    if (rl < data_.size()) {
      // rr is invalid
      const size_t l_max = less_(data_[lr], data_[ll]) ? ll : lr;
      return less_(data_[rl], data_[l_max]) ? l_max : rl;
    }
    if (lr < data_.size()) {
      // no right grandchildren
      const size_t l_max = less_(data_[lr], data_[ll]) ? ll : lr;
      return less_(data_[l_max], data_[right]) ? right : l_max;
    }
    if (ll < data_.size()) {
      // single left grandchild
      return less_(data_[ll], data_[right]) ? right : ll;
    }
    if (right < data_.size()) {
      // only children
      return less_(data_[right], data_[left]) ? left : right;
    }
    if (left < data_.size()) {
      // single left child
      return left;
    }
    return 0;
  }

  void pushUp(size_t index) {
    if (index == 0) {
      return;
    }
    size_t parent = getParent(index);
    if (isOnMinLevel(index)) {
      if (less_(data_[parent], data_[index])) {
        std::swap(data_[index], data_[parent]);
        pushUpMax(parent);
      } else {
        pushUpMin(index);
      }
    } else {
      if (less_(data_[index], data_[parent])) {
        std::swap(data_[index], data_[parent]);
        pushUpMin(parent);
      } else {
        pushUpMax(index);
      }
    }
  }

  void pushUpMin(size_t index) {
    while (index != 0 && getParent(index) != 0) {
      size_t grandparent = getParent(getParent(index));
      if (less_(data_[index], data_[grandparent])) {
        std::swap(data_[index], data_[grandparent]);
        index = grandparent;
        continue;
      }
      break;
    }
  }

  void pushUpMax(size_t index) {
    while (index != 0 && getParent(index) != 0) {
      size_t grandparent = getParent(getParent(index));
      if (less_(data_[grandparent], data_[index])) {
        std::swap(data_[index], data_[grandparent]);
        index = grandparent;
        continue;
      }
      break;
    }
  }

  void pushDown(size_t index) {
    if (isOnMinLevel(index)) {
      pushDownMin(index);
    } else {
      pushDownMax(index);
    }
  }

  void pushDownMin(size_t index) {
    while (true) {
      size_t min = getSmallestChildOrGrandchild(index);
      if (min == 0) {
        return;
      }
      size_t parent = getParent(min);
      if (parent != index) {
        // grandchild
        if (less_(data_[min], data_[index])) {
          std::swap(data_[min], data_[index]);
          if (less_(data_[parent], data_[min])) {
            std::swap(data_[parent], data_[min]);
          }
          index = min;
          continue;
        }
      } else if (less_(data_[min], data_[index])) {
        std::swap(data_[min], data_[index]);
      }
      break;
    }
  }

  void pushDownMax(size_t index) {
    while (true) {
      size_t max = getLargestChildOrGrandchild(index);
      if (max == 0) {
        return;
      }
      size_t parent = getParent(max);
      if (parent != index) {
        // grandchild
        if (less_(data_[index], data_[max])) {
          std::swap(data_[max], data_[index]);
          if (less_(data_[max], data_[parent])) {
            std::swap(data_[parent], data_[max]);
          }
          index = max;
          continue;
        }
      } else if (less_(data_[index], data_[max])) {
        std::swap(data_[max], data_[index]);
      }
      break;
    }
  }

  std::vector<T> data_;
  Comparator less_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
