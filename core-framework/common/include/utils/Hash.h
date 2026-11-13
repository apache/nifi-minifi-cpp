// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <cstddef>

namespace org::apache::nifi::minifi::utils {

// from the boost hash_combine docs
inline size_t hash_combine(size_t seed, size_t new_hash) noexcept {
  return seed ^ (new_hash + 0x9e3779b9 + (seed << 6U) + (seed >> 2U));
}

}  // namespace org::apache::nifi::minifi::utils
