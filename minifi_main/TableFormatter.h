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

#include <string>
#include <utility>
#include <vector>

#include "core/PropertyValue.h"

namespace org::apache::nifi::minifi::docs {

class Table {
 public:
  explicit Table(std::vector<std::string> header) : header_{std::move(header)} {}
  void addRow(std::vector<std::string> row);
  [[nodiscard]] std::string toString() const;

 private:
  [[nodiscard]] std::vector<size_t> find_widths() const;

  std::vector<std::string> header_;
  std::vector<std::vector<std::string>> rows_;
};

std::string formatName(const std::string& name, bool is_required);
std::string formatAllowableValues(const std::vector<org::apache::nifi::minifi::core::PropertyValue>& values);
std::string formatDescription(std::string description, bool supports_expression_language = false);
std::string formatSeparator(const std::vector<size_t>& widths);
std::string formatRow(const std::vector<std::string>& items, const std::vector<size_t>& widths);

}  // namespace org::apache::nifi::minifi::docs
