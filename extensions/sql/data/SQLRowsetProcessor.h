// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <vector>

#include "SQLRowSubscriber.h"
#include "DatabaseConnectors.h"

namespace org::apache::nifi::minifi::sql {

class SQLRowsetProcessor {
 public:
  SQLRowsetProcessor(std::unique_ptr<Rowset> rowset, std::vector<std::reference_wrapper<SQLRowSubscriber>> row_subscribers);

  size_t process(size_t max);

 private:
  void addRow(const Row& row, size_t rowCount);

  template <typename T>
  void processColumn(const std::string& name, const T& value) const {
    for (const auto& subscriber : row_subscribers_) {
      subscriber.get().processColumn(name, value);
    }
  }

 private:
  std::unique_ptr<Rowset> rowset_;
  std::vector<std::reference_wrapper<SQLRowSubscriber>> row_subscribers_;
};

}  // namespace org::apache::nifi::minifi::sql
