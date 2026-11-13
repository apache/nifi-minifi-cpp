// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace org::apache::nifi::minifi::docs {

class Table {
 public:
  explicit Table(std::vector<std::string> header) : header_{std::move(header)} {}
  void addRow(std::vector<std::string> row);
  [[nodiscard]] std::string toString() const;

 private:
  [[nodiscard]] std::vector<size_t> findWidths() const;

  std::vector<std::string> header_;
  std::vector<std::vector<std::string>> rows_;
};

}  // namespace org::apache::nifi::minifi::docs
