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
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "utils/ChecksumCalculator.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

class ConfigurationChecksums : public ResponseNode {
 public:
  ConfigurationChecksums() = default;
  explicit ConfigurationChecksums(const std::string& name, const utils::Identifier& uuid = {}) : ResponseNode(name, uuid) {}

  void addChecksumCalculator(utils::ChecksumCalculator& checksum_calculator);

  std::string getName() const override { return "configurationChecksums"; }
  std::vector<SerializedResponseNode> serialize() override;

 private:
  std::vector<gsl::not_null<utils::ChecksumCalculator*>> checksum_calculators_;
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
