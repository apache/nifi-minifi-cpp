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

#ifndef LIBMINIFI_INCLUDE_UTILS_FLATMAP_H_
#define LIBMINIFI_INCLUDE_UTILS_FLATMAP_H_

#include <tuple>
#include <functional>
#include <vector>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename K, typename V>
class FlatMap{
 public:
  using value_type = std::pair<K, V>;

 private:
  using Container = std::vector<value_type>;

 public:
  class iterator{
    friend class const_iterator;
    friend class FlatMap;
    explicit iterator(typename Container::iterator it): it_(it) {}

   public:
    using difference_type = void;
    using value_type = FlatMap::value_type;
    using pointer = void;
    using reference = void;
    using iterator_category = void;

    value_type* operator->() const {return &(*it_);}
    value_type& operator*() const {return *it_;}

    bool operator==(const iterator& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    iterator& operator++() {
      ++it_;
      return *this;
    }
   private:
    typename Container::iterator it_;
  };

  class const_iterator{
    friend class FlatMap;
    explicit const_iterator(typename Container::const_iterator it): it_(it) {}

   public:
    const_iterator(iterator it): it_(it.it_) {}  // NOLINT
    using difference_type = void;
    using value_type = const FlatMap::value_type;
    using pointer = void;
    using reference = void;
    using iterator_category = void;

    value_type* operator->() const {return &(*it_);}
    value_type& operator*() const {return *it_;}

    bool operator==(const const_iterator& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

    const_iterator& operator++() {
      ++it_;
      return *this;
    }
   private:
    typename Container::const_iterator it_;
  };

  FlatMap() = default;
  FlatMap(const FlatMap&) = default;
  FlatMap(FlatMap&&) noexcept = default;
  FlatMap(std::initializer_list<value_type> items) : data_(items) {}
  template<class InputIterator>
  FlatMap(InputIterator begin, InputIterator end) : data_(begin, end) {}

  FlatMap& operator=(const FlatMap& source) = default;
  FlatMap& operator=(FlatMap&& source) = default;
  FlatMap& operator=(std::initializer_list<value_type> items) {
    data_ = items;
    return *this;
  }

  std::size_t size() {
    return data_.size();
  }

  V& operator[](const K& key) {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }
    data_.emplace_back(key, V{});
    return data_.rbegin()->second;
  }

  iterator erase(const_iterator pos) {
    auto offset = pos.it_ - data_.begin();
    std::swap(*data_.rbegin(), *(data_.begin() + offset));
    data_.pop_back();
    return iterator{data_.begin() + offset};
  }

  std::size_t erase(const K& key) {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->first == key) {
        std::swap(*data_.rbegin(), *it);
        data_.pop_back();
        return 1;
      }
    }
    return 0;
  }

  std::pair<iterator, bool> insert(const value_type& value) {
    auto it = find(value.first);
    if (it != end()) {
      return {it, false};
    }
    data_.push_back(value);
    return {iterator{data_.begin() + data_.size() - 1}, true};
  }

  template<typename M>
  std::pair<iterator, bool> insert_or_assign(const K& key, M&& value) {
    auto it = find(key);
    if (it != end()) {
      it->second = std::forward<M>(value);
      return {it, false};
    }
    data_.emplace_back(key, std::forward<M>(value));
    return {iterator{data_.begin() + data_.size() - 1}, true};
  }

  iterator find(const K& key) {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->first == key) return iterator{it};
    }
    return end();
  }

  const_iterator find(const K& key) const {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->first == key) return const_iterator{it};
    }
    return end();
  }

  iterator begin() {
    return iterator{data_.begin()};
  }

  iterator end() {
    return iterator{data_.end()};
  }

  const_iterator begin() const {
    return const_iterator{data_.begin()};
  }

  const_iterator end() const {
    return const_iterator{data_.end()};
  }

 private:
  Container data_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_FLATMAP_H_
