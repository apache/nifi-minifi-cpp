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

 private:
  const char* begin_;
  const char* end_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
