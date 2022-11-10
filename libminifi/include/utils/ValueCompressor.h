/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenseas/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <optional>
#include <unordered_map>
#include <vector>
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

template<typename T, typename CompressedType = size_t>
class ValueCompressor {
 public:
  using compressed_type = CompressedType;
  static constexpr compressed_type INVALID = static_cast<compressed_type>(-1);

  [[nodiscard]]
  compressed_type compress(const T& value) {
    auto [it, inserted] = value_to_id_.insert({value, gsl::narrow<compressed_type>(values_.size())});
    if (inserted) {
      values_.push_back(value);
    }
    return it->second;
  }

  [[nodiscard]]
  std::optional<T> decompress(const compressed_type& id) const {
    const auto idx = gsl::narrow<size_t>(id);
    if (idx < values_.size()) {
      return values_[idx];
    }
    return std::nullopt;
  }

  void clear() {
    value_to_id_.clear();
    values_.clear();
  }

 private:
  std::unordered_map<T, compressed_type> value_to_id_;
  std::vector<T> values_;
};

}  // namespace org::apache::nifi::minifi::utils