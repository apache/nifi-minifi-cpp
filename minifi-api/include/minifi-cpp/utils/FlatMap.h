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

#pragma once

#include <tuple>
#include <functional>
#include <vector>
#include <utility>

namespace org::apache::nifi::minifi::utils {

template<typename K, typename V>
class FlatMap{
  using Container = std::vector<std::pair<K, V>>;

 public:
  using value_type = std::pair<K, V>;
  using reference = value_type&;
  using const_reference = const value_type&;
  using difference_type = typename Container::difference_type;
  using size_type = typename Container::size_type;

  class iterator{
    friend class const_iterator;
    friend class FlatMap;
    explicit iterator(typename Container::iterator it) noexcept : it_(it) {}

   public:
    using difference_type = typename Container::iterator::difference_type;
    using value_type = FlatMap::value_type;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::forward_iterator_tag;

    iterator() = default;

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

    iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

   private:
    typename Container::iterator it_;
  };

  class const_iterator{
    friend class FlatMap;
    explicit const_iterator(typename Container::const_iterator it) noexcept : it_(it) {}

   public:
    const_iterator(iterator it) noexcept : it_(it.it_) {}  // NOLINT
    const_iterator() = default;

    using difference_type = typename Container::const_iterator::difference_type;
    using value_type = const FlatMap::value_type;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::forward_iterator_tag;

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

    const_iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
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
  FlatMap& operator=(FlatMap&& source) noexcept = default;
  FlatMap& operator=(std::initializer_list<value_type> items) {
    data_ = items;
    return *this;
  }

  size_type size() const noexcept {
    return data_.size();
  }

  template<typename T>
  requires std::constructible_from<K, T> && std::equality_comparable_with<K, T>
  V& operator[](const T& key) {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }
    data_.emplace_back(key, V{});
    return data_.rbegin()->second;
  }

  template<std::equality_comparable_with<K> T>
  const V& at(const T& key) const {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }
    throw std::out_of_range("utils::FlatMap::at");
  }

  template<std::equality_comparable_with<K> T>
  V& at(const T& key) {
    auto it = find(key);
    if (it != end()) {
      return it->second;
    }
    throw std::out_of_range("utils::FlatMap::at");
  }

  iterator erase(const_iterator pos) {
    auto offset = pos.it_ - data_.begin();
    std::swap(*data_.rbegin(), *(data_.begin() + offset));
    data_.pop_back();
    return iterator{data_.begin() + offset};
  }

  template<std::equality_comparable_with<K> T>
  std::size_t erase(const T& key) {
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

  template<typename M>
  std::pair<iterator, bool> insert_or_assign(K&& key, M&& value) {
    auto it = find(key);
    if (it != end()) {
      it->second = std::forward<M>(value);
      return {it, false};
    }
    data_.emplace_back(std::move(key), std::forward<M>(value));
    return {iterator{data_.begin() + data_.size() - 1}, true};
  }

  template<std::equality_comparable_with<K> T>
  iterator find(const T& key) {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->first == key) return iterator{it};
    }
    return end();
  }

  template<std::equality_comparable_with<K> T>
  const_iterator find(const T& key) const {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->first == key) return const_iterator{it};
    }
    return end();
  }

  iterator begin() noexcept {
    return iterator{data_.begin()};
  }

  iterator end() noexcept {
    return iterator{data_.end()};
  }

  const_iterator begin() const noexcept {
    return const_iterator{data_.begin()};
  }

  const_iterator end() const noexcept {
    return const_iterator{data_.end()};
  }

  const_iterator cbegin() const noexcept {
    return const_iterator{data_.begin()};
  }

  const_iterator cend() const noexcept {
    return const_iterator{data_.end()};
  }

  bool operator==(const FlatMap& other) const {
    if (size() != other.size()) {
      return false;
    }
    // no linearity can be guaranteed unless we
    // sort the underlying storage
    for (const auto& item : *this) {
      auto it = other.find(item.first);
      if (it == other.end() || it.second != item.second) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const FlatMap& other) const {
    return !(*this == other);
  }

  void swap(FlatMap& other) {
    using std::swap;
    swap(data_, other.data_);
  }

  friend void swap(FlatMap& lhs, FlatMap& rhs) {
    lhs.swap(rhs);
  }

  size_type max_size() const noexcept {
    return data_.max_size();
  }

  [[nodiscard]] bool empty() const noexcept {
    return data_.empty();
  }

  template<std::equality_comparable_with<K> T>
  bool contains(const T& key) const {
    return find(key) != end();
  }

 private:
  Container data_;
};

}  // namespace org::apache::nifi::minifi::utils
