// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "utils/ArrayUtils.h"

#if defined(_MSC_VER)
constexpr std::string_view removeStructOrClassPrefix(std::string_view input) {
  using namespace std::literals;
  for (auto prefix : { "struct "sv, "class "sv }) {
    if (input.find(prefix) == 0) {
      return input.substr(prefix.size());
    }
  }
  return input;
}
#endif

// based on https://bitwizeshift.github.io/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/
template<typename T>
constexpr auto typeNameArray() {  // In root namespace to avoid gcc-13 optimizing out namespaces from __PRETTY_FUNCTION__
#if defined(__clang__)
  constexpr auto prefix   = std::string_view{"[T = "};
  constexpr auto suffix   = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
  constexpr auto prefix   = std::string_view{"with T = "};
  constexpr auto suffix   = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
  constexpr auto prefix   = std::string_view{"typeNameArray<"};
  constexpr auto suffix   = std::string_view{">(void)"};
  constexpr auto function = std::string_view{__FUNCSIG__};
#else
# error Unsupported compiler
#endif

  constexpr auto start = function.find(prefix) + prefix.size();
  constexpr auto end = function.rfind(suffix);
  static_assert(start < end);

#if defined(_MSC_VER)
  constexpr auto name = removeStructOrClassPrefix(function.substr(start, end - start));
#else
  constexpr auto name = function.substr(start, end - start);
#endif

  return org::apache::nifi::minifi::utils::string_view_to_array<name.length()>(name);
}

namespace org::apache::nifi::minifi::core {

template<typename T>
struct TypeNameHolder {
  static constexpr auto value = typeNameArray<T>();
};

template<typename T>
constexpr std::string_view className() {
  return utils::array_to_string_view(TypeNameHolder<std::remove_reference_t<T>>::value);
}

}  // namespace org::apache::nifi::minifi::core
