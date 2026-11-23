// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <array>
#include <ostream>
#include <string>
#include <utility>
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "minifi-cpp/utils/SmallString.h"

// SmallString derives from std::array, so fmt would also consider it a range, making the formatter below ambiguous
template <size_t N, typename Char>
struct fmt::range_format_kind<org::apache::nifi::minifi::utils::SmallString<N>, Char>
    : std::integral_constant<fmt::range_format, fmt::range_format::disabled> {};

template <size_t N>
struct fmt::formatter<org::apache::nifi::minifi::utils::SmallString<N>> {
  formatter<std::string_view> string_view_formatter;

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return string_view_formatter.parse(ctx);
  }

  template <typename FormatContext>
  auto format(const org::apache::nifi::minifi::utils::SmallString<N>& small_string, FormatContext& ctx) const {
    return string_view_formatter.format(small_string.view(), ctx);
  }
};
