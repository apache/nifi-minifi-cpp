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

#include "ConfigFile.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "utils/StringUtils.h"

namespace {
constexpr std::array<const char*, 2> DEFAULT_SENSITIVE_PROPERTIES{"nifi.security.client.pass.phrase",
                                                                  "nifi.rest.api.password"};
constexpr const char* ADDITIONAL_SENSITIVE_PROPS_PROPERTY_NAME = "nifi.sensitive.props.additional.keys";
}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

std::vector<std::string> ConfigFile::getSensitiveProperties() const {
  std::vector<std::string> sensitive_properties(DEFAULT_SENSITIVE_PROPERTIES.begin(), DEFAULT_SENSITIVE_PROPERTIES.end());
  const utils::optional<std::string> additional_sensitive_props_list = getValue(ADDITIONAL_SENSITIVE_PROPS_PROPERTY_NAME);
  if (additional_sensitive_props_list) {
    std::vector<std::string> additional_sensitive_properties = utils::StringUtils::split(*additional_sensitive_props_list, ",");
    sensitive_properties = mergeProperties(sensitive_properties, additional_sensitive_properties);
  }

  const auto not_found = [this](const std::string& property_name) { return !hasValue(property_name); };
  const auto new_end = std::remove_if(sensitive_properties.begin(), sensitive_properties.end(), not_found);
  sensitive_properties.erase(new_end, sensitive_properties.end());

  return sensitive_properties;
}

std::vector<std::string> ConfigFile::mergeProperties(std::vector<std::string> properties,
                                                     const std::vector<std::string>& additional_properties) {
  for (const auto& property_name : additional_properties) {
    std::string property_name_trimmed = utils::StringUtils::trim(property_name);
    if (!property_name_trimmed.empty()) {
      properties.push_back(std::move(property_name_trimmed));
    }
  }

  std::sort(properties.begin(), properties.end());
  auto new_end = std::unique(properties.begin(), properties.end());
  properties.erase(new_end, properties.end());
  return properties;
}

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
