// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <vector>

namespace org::apache::nifi::minifi::sql {

struct SQLRowSubscriber {
  virtual ~SQLRowSubscriber() = default;
  virtual void beginProcessBatch() = 0;
  virtual void endProcessBatch() = 0;
  virtual void beginProcessRow() = 0;
  virtual void endProcessRow() = 0;
  virtual void finishProcessing() = 0;
  virtual void processColumnNames(const std::vector<std::string>& names) = 0;
  virtual void processColumn(const std::string& name, const std::string& value) = 0;
  virtual void processColumn(const std::string& name, double value) = 0;
  virtual void processColumn(const std::string& name, int value) = 0;
  virtual void processColumn(const std::string& name, long long value) = 0;  // NOLINT(runtime/int)
  virtual void processColumn(const std::string& name, unsigned long long value) = 0;  // NOLINT(runtime/int)
  // Process NULL value.
  virtual void processColumn(const std::string& name, const char* value) = 0;
};

}  // namespace org::apache::nifi::minifi::sql
