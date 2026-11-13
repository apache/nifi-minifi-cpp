// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once
#include <type_traits>

namespace org::apache::nifi::minifi::utils::meta {
template<typename... Types>
struct type_list {
  template<typename T>
  [[nodiscard]] constexpr static bool contains() noexcept {
    return (std::is_same_v<T, Types> || ...);
  }
};
}  // namespace org::apache::nifi::minifi::utils::meta
