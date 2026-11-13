// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include "minifi-cpp/core/RepositoryMetricsSource.h"

namespace org::apache::nifi::minifi::core {

class RepositoryMetricsSourceImpl : public virtual RepositoryMetricsSource {
 public:
  uint64_t getMaxRepositorySize() const override {
    return 0;
  }

  bool isFull() const override {
    return false;
  }

  bool isRunning() const override {
    return true;
  }

  std::optional<RocksDbStats> getRocksDbStats() const override {
    return std::nullopt;
  }
};

}  // namespace org::apache::nifi::minifi::core
