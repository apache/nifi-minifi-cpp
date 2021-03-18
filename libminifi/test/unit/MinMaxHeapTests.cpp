/**
 *
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

#include "MinMaxHeap.h"
#include <queue>
#include <random>

#include "../TestBase.h"

using org::apache::nifi::minifi::utils::MinMaxHeap;

enum class Operations {
  PopMin,
  PopMax,
  Push
};

struct MinMaxHeapTestAccessor {
  template<typename T, typename Comparator>
  static void verify(const MinMaxHeap<T, Comparator>& heap) {
    verifyIndex<T, Comparator>(heap, 0, nullptr, nullptr, true);
  }

  template<typename T, typename Comparator>
  static void verifyIndex(const MinMaxHeap<T, Comparator>& heap, size_t index, const T* min, const T* max, bool is_min_level) {
    if (index >= heap.data_.size()) {
      return;
    }
    if (min != nullptr) {
      REQUIRE(!heap.less_(heap.data_[index], *min));
    }
    if (max != nullptr) {
      REQUIRE(!heap.less_(*max, heap.data_[index]));
    }
    if (is_min_level) {
      verifyIndex(heap, 2 * index + 1, &heap.data_[index], max, !is_min_level);
      verifyIndex(heap, 2 * index + 2, &heap.data_[index], max, !is_min_level);
    } else {
      verifyIndex(heap, 2 * index + 1, min, &heap.data_[index], !is_min_level);
      verifyIndex(heap, 2 * index + 2, min, &heap.data_[index], !is_min_level);
    }
  }
};

template<typename T>
struct TestHeap {
  const T& max() const {
    const T& max = heap.max();
    REQUIRE(max == items.back());
    return max;
  }

  const T& min() const {
    const T& min = heap.min();
    REQUIRE(min == items.front());
    return min;
  }

  void popMin() {
    min();
    heap.popMin();
    MinMaxHeapTestAccessor::verify(heap);
    items.erase(items.begin());
    ops.push_back(Operations::PopMin);
  }

  void popMax() {
    max();
    heap.popMax();
    MinMaxHeapTestAccessor::verify(heap);
    items.pop_back();
    ops.push_back(Operations::PopMax);
  }

  void clear() {
    heap.clear();
    items.clear();
    ops.clear();
  }

  void push(T item) {
    heap.push(item);
    MinMaxHeapTestAccessor::verify(heap);
    items.insert(std::lower_bound(items.begin(), items.end(), item), item);
    min();
    max();
    ops.push_back(Operations::Push);
  }

  bool empty() const {
    bool is_empty = heap.empty();
    REQUIRE(is_empty == items.empty());
    return is_empty;
  }

  size_t size() const {
    size_t size = heap.size();
    REQUIRE(size == items.size());
    return size;
  }

  MinMaxHeap<T> heap;
  std::vector<T> items;
  std::vector<Operations> ops;
};

std::vector<int> createRandomInts(size_t N) {
  std::vector<int> result(N);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dis;
  for (size_t i = 0; i < N; ++i) {
    result.push_back(dis(gen));
  }
  return result;
}

bool rollDice(size_t percentage = 50) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dis{0, 100};
  return dis(gen) < percentage;
}

TEST_CASE("MinMaxHeap is consistent with a sorted vector", "[MinMaxHeap1]") {
  TestHeap<int> heap;
  std::vector<std::vector<int>> test_sets{
      {1, 2, 3, 4, 5},
      {1, 2, 5, 3, 4},
      {5, 4, 3, 2, 1},
      {5, 4, 5, 4, 1},
      {1, 1, 3, 2, 1},
      createRandomInts(100)
  };

  for (auto& test_set : test_sets) {
    heap.clear();
    REQUIRE(heap.empty());
    for (auto& item : test_set) {
      heap.push(item);
      if (rollDice(10)) {
        heap.popMax();
      } else if (rollDice(10)) {
        heap.popMin();
      }
    }
    while (!heap.empty()) {
      if (rollDice()) {
        heap.popMin();
      } else {
        heap.popMax();
      }
    }
  }
}



