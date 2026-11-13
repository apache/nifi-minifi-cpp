// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "utils/ChecksumCalculator.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::state::response {

class ConfigurationChecksums : public ResponseNodeImpl {
 public:
  ConfigurationChecksums() = default;
  explicit ConfigurationChecksums(const std::string_view name, const utils::Identifier& uuid = {}) : ResponseNodeImpl(name, uuid) {}

  MINIFIAPI static constexpr const char* Description = "Metric node that defines checksums of configuration files in the C2 protocol";

  void addChecksumCalculator(utils::ChecksumCalculator& checksum_calculator);

  std::string getName() const override { return "configurationChecksums"; }
  std::vector<SerializedResponseNode> serialize() override;

 private:
  std::vector<gsl::not_null<utils::ChecksumCalculator*>> checksum_calculators_;
};

}  // namespace org::apache::nifi::minifi::state::response
