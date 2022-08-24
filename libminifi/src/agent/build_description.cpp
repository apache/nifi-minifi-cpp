/**
 *
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

#include "agent/build_description.h"

namespace org::apache::nifi::minifi {

std::vector<BundleDetails>& ExternalBuildDescription::getExternal() {
  static std::vector<BundleDetails> external_groups;
  return external_groups;
}

std::map<std::string, Components>& ExternalBuildDescription::getExternalMappings() {
  static std::map<std::string, Components> external_mappings;
  return external_mappings;
}

void ExternalBuildDescription::addExternalComponent(const BundleDetails& details, const ClassDescription& description) {
  bool found = false;
  for (const auto &d : getExternal()) {
    if (d.artifact == details.artifact) {
      found = true;
      break;
    }
  }
  if (!found) {
    getExternal().push_back(details);
  }
  if (description.type_ == ResourceType::Processor) {
    getExternalMappings()[details.artifact].processors_.push_back(description);
  } else if (description.type_ == ResourceType::ControllerService) {
    getExternalMappings()[details.artifact].controller_services_.push_back(description);
  } else {
    getExternalMappings()[details.artifact].other_components_.push_back(description);
  }
}

Components ExternalBuildDescription::getClassDescriptions(const std::string &group) {
  return getExternalMappings()[group];
}

std::vector<BundleDetails> ExternalBuildDescription::getExternalGroups() {
  return getExternal();
}

}  // namespace org::apache::nifi::minifi
