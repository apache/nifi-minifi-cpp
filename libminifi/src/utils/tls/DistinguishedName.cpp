// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
      [](const std::string& component) { return component.starts_with("CN="); });
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
