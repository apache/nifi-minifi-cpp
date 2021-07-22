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

#include "core/state/nodes/ConfigurationChecksums.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

void ConfigurationChecksums::addChecksumCalculator(utils::ChecksumCalculator& checksum_calculator) {
  checksum_calculators_.push_back(gsl::make_not_null(&checksum_calculator));
}

std::vector<SerializedResponseNode> ConfigurationChecksums::serialize() {
  SerializedResponseNode checksums_node;
  checksums_node.name = utils::ChecksumCalculator::CHECKSUM_TYPE;
  checksums_node.children.reserve(checksum_calculators_.size());

  for (auto checksum_calculator : checksum_calculators_) {
    SerializedResponseNode file_checksum_node;
    file_checksum_node.name = checksum_calculator->getFileName();
    file_checksum_node.value = checksum_calculator->getChecksum();
    checksums_node.children.push_back(file_checksum_node);
  }

  return std::vector<SerializedResponseNode>{checksums_node};
}

REGISTER_RESOURCE(ConfigurationChecksums, "Node part of an AST that defines checksums of configuration files in the C2 protocol");

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
