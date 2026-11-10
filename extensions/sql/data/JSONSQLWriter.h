// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <functional>

#include "rapidjson/document.h"

#include "SQLWriter.h"

namespace org::apache::nifi::minifi::sql {

class JSONSQLWriter: public SQLWriter {
 public:
  using ColumnFilter = std::function<bool(const std::string&)>;

  explicit JSONSQLWriter(bool pretty, ColumnFilter column_filter = [] (const std::string&) {return true;});

  std::string toString() override;

 private:
  void beginProcessBatch() override;
  void endProcessBatch() override;
  void beginProcessRow() override;
  void endProcessRow() override;
  void finishProcessing() override;
  void processColumnNames(const std::vector<std::string>& name) override;
  void processColumn(const std::string& name, const std::string& value) override;
  void processColumn(const std::string& name, double value) override;
  void processColumn(const std::string& name, int value) override;
  void processColumn(const std::string& name, long long value) override;  // NOLINT(runtime/int)
  void processColumn(const std::string& name, unsigned long long value) override;  // NOLINT(runtime/int)
  void processColumn(const std::string& name, const char* value) override;

  void addToJSONRow(const std::string& column_name, rapidjson::Value&& json_value);

  rapidjson::Value toJSONString(const std::string& s);

 private:
  bool pretty_;
  rapidjson::Document current_batch_;
  rapidjson::Value current_row_;
  ColumnFilter column_filter_;
};

}  // namespace org::apache::nifi::minifi::sql
