// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/nodes/ConfigurationChecksums.h"
#include "core/Resource.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::state::response {

void ConfigurationChecksums::addChecksumCalculator(utils::ChecksumCalculator& checksum_calculator) {
  checksum_calculators_.push_back(gsl::make_not_null(&checksum_calculator));
}

std::vector<SerializedResponseNode> ConfigurationChecksums::serialize() {
  SerializedResponseNode checksums_node;
  checksums_node.name = utils::ChecksumCalculator::CHECKSUM_TYPE;
  checksums_node.children.reserve(checksum_calculators_.size());

  for (auto checksum_calculator : checksum_calculators_) {
    SerializedResponseNode file_checksum_node;
    file_checksum_node.name = checksum_calculator->getFileNameOfFirstFileLocation().string();
    file_checksum_node.value = checksum_calculator->getChecksum();
    checksums_node.children.push_back(file_checksum_node);
  }

  return std::vector<SerializedResponseNode>{checksums_node};
}

REGISTER_RESOURCE(ConfigurationChecksums, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
