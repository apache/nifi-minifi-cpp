// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "agent/agent_docs.h"

namespace org::apache::nifi::minifi {

namespace {
std::map<BundleCoordinate, Components>& getAgentDocsClassMappings() {
  static std::map<BundleCoordinate, Components> mappings;
  return mappings;
}
}  // namespace

const std::map<BundleCoordinate, Components>& ClassDescriptionRegistry::getClassDescriptions() {
  return getAgentDocsClassMappings();
}

std::map<BundleCoordinate, Components>& ClassDescriptionRegistry::getMutableClassDescriptions() {
  return getAgentDocsClassMappings();
}

void ClassDescriptionRegistry::clearClassDescriptionsForBundle(const std::string& bundle_name) {
  std::erase_if(getAgentDocsClassMappings(), [&bundle_name](const auto& entry) {
    return entry.first.name == bundle_name;
  });
}

Components& ClassDescriptionRegistry::getMutableComponents(const BundleCoordinate& bundle_coordinate) {
  const auto [it, success] = getMutableClassDescriptions().try_emplace(bundle_coordinate, bundle_coordinate);
  (void)success;
  return it->second;
}


}  // namespace org::apache::nifi::minifi
