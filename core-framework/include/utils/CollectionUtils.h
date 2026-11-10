// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <algorithm>
#include <utility>

namespace org::apache::nifi::minifi::utils {

namespace internal {

template<typename T, typename Arg, typename = void>
struct find_in_range {
  static auto call(const T& range, const Arg& arg) -> decltype(std::find(range.begin(), range.end(), arg)) {
    return std::find(range.begin(), range.end(), arg);
  }
};

template<typename T, typename Arg>
struct find_in_range<T, Arg, decltype(std::declval<const T&>().find(std::declval<const Arg&>()), void())> {
  static auto call(const T& range, const Arg& arg) -> decltype(range.find(arg)) {
    return range.find(arg);
  }
};

}  // namespace internal

template<typename T, typename U>
bool haveCommonItem(const T& a, const U& b) {
  using Item = typename T::value_type;
  return std::any_of(a.begin(), a.end(), [&] (const Item& item) {
    return internal::find_in_range<U, Item>::call(b, item) != b.end();
  });
}

}  // namespace org::apache::nifi::minifi::utils
