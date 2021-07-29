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

#include <utility>
#include <string>

#include "utils/file/FileUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class Path {
 public:
  Path() = default;

  explicit Path(std::string value): value_(std::move(value)) {}

  Path& operator=(const std::string& new_value) {
    value_ = new_value;
    return *this;
  }

  template<typename T>
  Path& operator/=(T&& suffix) {
    value_ = file::concat_path(value_, std::string{std::forward<T>(suffix)});
    return *this;
  }

  template<typename T>
  Path operator/(T&& suffix) {
    return Path{file::concat_path(value_, std::string{std::forward<T>(suffix)})};
  }

  std::string str() const {
    return value_;
  }

  explicit operator std::string() const {
    return value_;
  }

  bool empty() const noexcept {
    return value_.empty();
  }

 private:
  std::string value_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
