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

#include "TableFormatter.h"

#include "range/v3/view/transform.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/range/conversion.hpp"

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

std::string formatName(const std::string& name, bool is_required) {
  if (is_required) {
    return "**" + name + "**";
  } else {
    return name;
  }
}

std::string formatAllowableValues(const std::vector<org::apache::nifi::minifi::core::PropertyValue>& values) {
  return values
      | ranges::views::transform([](const auto& value) { return value.to_string(); })
      | ranges::views::join(std::string_view{"<br/>"})
      | ranges::to<std::string>();
}

std::string formatDescription(std::string description, bool supports_expression_language) {
  org::apache::nifi::minifi::utils::StringUtils::replaceAll(description, "\n", "<br/>");
  return supports_expression_language ? description + "<br/>**Supports Expression Language: true**" : description;
}

std::string formatSeparator(const std::vector<size_t>& widths) {
  std::string result = widths
      | ranges::views::transform([](size_t width) { return std::string(width + 2, '-'); })
      | ranges::views::join('|')
      | ranges::to<std::string>();
  return '|' + result + "|\n";
}

std::string formatRow(const std::vector<std::string>& items, const std::vector<size_t>& widths) {
  std::string result;
  for (size_t i = 0; i < items.size(); ++i) {
    size_t padding = widths[i] - items[i].length() + 1;
    result.append("| ").append(items[i]).append(std::string(padding, ' '));
  }
  return result + "|\n";
}

}  // namespace org::apache::nifi::minifi::docs
