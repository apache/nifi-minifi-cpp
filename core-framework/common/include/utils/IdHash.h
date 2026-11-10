// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
