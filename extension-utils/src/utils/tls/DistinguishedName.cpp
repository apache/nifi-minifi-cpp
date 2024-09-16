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

#include "utils/tls/DistinguishedName.h"

#include <algorithm>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::tls {

DistinguishedName::DistinguishedName(const std::vector<std::string>& components) {
  std::transform(components.begin(), components.end(), std::back_inserter(components_),
      [](const std::string& component) { return utils::string::trim(component); });
  std::sort(components_.begin(), components_.end());
}

DistinguishedName DistinguishedName::fromCommaSeparated(const std::string& comma_separated_components) {
  return DistinguishedName{utils::string::splitRemovingEmpty(comma_separated_components, ",")};
}

DistinguishedName DistinguishedName::fromSlashSeparated(const std::string &slash_separated_components) {
  return DistinguishedName{utils::string::splitRemovingEmpty(slash_separated_components, "/")};
}

std::optional<std::string> DistinguishedName::getCN() const {
  const auto it = std::find_if(components_.begin(), components_.end(),
      [](const std::string& component) { return component.compare(0, 3, "CN=") == 0; });
  if (it != components_.end()) {
    return it->substr(3);
  } else {
    return std::nullopt;
  }
}

std::string DistinguishedName::toString() const {
  return utils::string::join(", ", components_);
}

}  // namespace org::apache::nifi::minifi::utils::tls
