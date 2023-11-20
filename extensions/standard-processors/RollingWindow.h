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

#include <chrono>
#include <mutex>
#include <queue>
#include <vector>

namespace org::apache::nifi::minifi::processors::standard::utils {

namespace detail {
template<typename T, typename Container, typename Comparator>
struct priority_queue : std::priority_queue<T, Container, Comparator> {
  using std::priority_queue<T, Container, Comparator>::priority_queue;

  // Expose the underlying container
  const Container& get_container() const & { return this->c; }
  Container get_container() && { return std::move(this->c); }
};
}  // namespace detail

template<typename Timestamp, typename Value>
class RollingWindow {
 public:
  struct Entry {
    Timestamp timestamp{};
    Value value{};
  };
  struct EntryComparator {
    // greater-than, because std::priority_queue order is reversed. This way, top() is the oldest entry.
    bool operator()(const Entry& lhs, const Entry& rhs) const {
      return lhs.timestamp > rhs.timestamp;
    }
  };

  void removeOlderThan(std::chrono::time_point<std::chrono::system_clock> timestamp) {
    while (!state_.empty() && state_.top().timestamp < timestamp) {
      state_.pop();
    }
  }

  /** Remove the oldest entries until the size is <= size. */
  void shrinkToSize(size_t size) {
    while (state_.size() > size && !state_.empty()) {
      state_.pop();
    }
  }

  void add(Timestamp timestamp, Value value) { state_.push({timestamp, value}); }

  std::vector<Entry> getEntries() const & { return state_.get_container(); }
  std::vector<Entry> getEntries() && { return std::move(state_).get_container(); }
 private:
  detail::priority_queue<Entry, std::vector<Entry>, EntryComparator> state_;
};

}  // namespace org::apache::nifi::minifi::processors::standard::utils
