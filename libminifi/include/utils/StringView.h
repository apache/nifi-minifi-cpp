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

#include <string>
#include <iterator>
#include <algorithm>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class StringView {
 public:
  constexpr StringView(const char* begin, const char* end) : begin_(begin), end_(end) {}
  explicit StringView(const char* str) : begin_(str), end_(begin_ + std::char_traits<char>::length(str)) {}
  explicit StringView(const std::string& str): begin_(&*str.begin()), end_(begin_ + str.length()) {}

  constexpr const char* begin() const noexcept {
    return begin_;
  }

  constexpr const char* end() const noexcept {
    return end_;
  }

  constexpr bool empty() const noexcept {
    return begin_ == end_;
  }

  std::reverse_iterator<const char*> rbegin() const noexcept {
    return std::reverse_iterator<const char*>{end_};
  }

  std::reverse_iterator<const char*> rend() const noexcept {
    return std::reverse_iterator<const char*>{begin_};
  }

  constexpr size_t size() const noexcept {
    return end_ - begin_;
  }

  constexpr size_t length() const noexcept {
    return size();
  }

  friend bool operator==(const StringView& lhs, const std::string& rhs) {
    if (lhs.length() != rhs.length()) {
      return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }

  friend bool operator==(const StringView& lhs, const char* rhs) {
    if (lhs.length() != std::char_traits<char>::length(rhs)) {
      return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs);
  }

  friend bool operator==(const StringView& lhs, const StringView& rhs) {
    if (lhs.length() != rhs.length()) {
      return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }

  friend bool operator==(const std::string& lhs, const StringView& rhs) {
    return rhs == lhs;
  }

  friend bool operator==(const char* lhs, const StringView& rhs) {
    return rhs == lhs;
  }

  friend bool operator!=(const StringView& lhs, const std::string& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const StringView& lhs, const char* rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const StringView& lhs, const StringView& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const std::string& lhs, const StringView& rhs) {
    return rhs != lhs;
  }

  friend bool operator!=(const char* lhs, const StringView& rhs) {
    return rhs != lhs;
  }

  bool startsWith(const char* prefix) const noexcept {
    return startsWith(StringView{prefix});
  }

  bool endsWith(const char* suffix) const noexcept {
    return endsWith(StringView{suffix});
  }

  bool startsWith(const std::string& prefix) const noexcept {
    return startsWith(StringView{prefix});
  }

  bool endsWith(const std::string& suffix) const noexcept {
    return endsWith(StringView{suffix});
  }

  bool startsWith(const StringView& prefix) const noexcept {
    if (prefix.length() > length()) {
      return false;
    }
    return std::equal(prefix.begin(), prefix.end(), begin());
  }

  bool endsWith(const StringView& suffix) const noexcept {
    if (suffix.length() > length())
      return false;
    return std::equal(suffix.rbegin(), suffix.rend(), rbegin());
  }

 private:
  const char* begin_;
  const char* end_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
