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

#include "minifi-cpp/utils/Id.h"
#include "utils/Hash.h"

namespace std {
template<>
struct hash<org::apache::nifi::minifi::utils::Identifier> {
  size_t operator()(const org::apache::nifi::minifi::utils::Identifier& id) const noexcept {
    static_assert(sizeof(org::apache::nifi::minifi::utils::Identifier) % sizeof(size_t) == 0);
    constexpr int slices = sizeof(org::apache::nifi::minifi::utils::Identifier) / sizeof(size_t);
    const auto get_slice = [](const org::apache::nifi::minifi::utils::Identifier& id, size_t idx) -> size_t {
      size_t result{};
      memcpy(&result, reinterpret_cast<const unsigned char*>(&id.data_) + idx * sizeof(size_t), sizeof(size_t));
      return result;
    };
    size_t hash = get_slice(id, 0);
    for (size_t i = 1; i < slices; ++i) {
      hash = org::apache::nifi::minifi::utils::hash_combine(hash, get_slice(id, i));
    }
    return hash;
  }
};
}  // namespace std
