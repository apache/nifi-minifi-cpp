// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "TableFormatter.h"

#include <algorithm>

#include "range/v3/view/transform.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/range/conversion.hpp"

#include "minifi-cpp/utils/gsl.h"

namespace {

std::string formatRow(const std::vector<std::string>& items, const std::vector<size_t>& widths) {
  std::string result;
  for (size_t i = 0; i < items.size(); ++i) {
    size_t padding = widths[i] - items[i].length() + 1;
    result.append("| ").append(items[i]).append(std::string(padding, ' '));
  }
  return result + "|\n";
}

std::string formatSeparator(const std::vector<size_t>& widths) {
  std::string result = widths
      | ranges::views::transform([](size_t width) { return std::string(width + 2, '-'); })
      | ranges::views::join('|')
      | ranges::to<std::string>();
  return '|' + result + "|\n";
}

}  // namespace

namespace org::apache::nifi::minifi::docs {

void Table::addRow(std::vector<std::string> row) {
  gsl_Expects(row.size() == header_.size());
  rows_.push_back(std::move(row));
}

std::vector<size_t> Table::findWidths() const {
  std::vector<size_t> widths(header_.size());
  for (size_t i = 0; i < header_.size(); ++i) {
    size_t width = header_[i].length();
    for (const auto& row : rows_) {
      width = std::max(width, row[i].length());
    }
    widths[i] = width;
  }
  return widths;
}

std::string Table::toString() const {
  std::vector<size_t> widths = findWidths();
  std::string result = formatRow(header_, widths);
  result.append(formatSeparator(widths));
  for (const auto& row : rows_) {
    result.append(formatRow(row, widths));
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::docs
