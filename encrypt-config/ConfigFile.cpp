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
#include <optional>

#include "utils/StringUtils.h"
#include "properties/Configuration.h"

namespace org::apache::nifi::minifi::encrypt_config {

std::vector<std::string> ConfigFile::getSensitiveProperties() const {
  auto sensitive_properties = Configuration::getSensitiveProperties([this](const std::string& sensitive_props) { return getValue(sensitive_props); });
  const auto not_found = [this](const std::string& property_name) { return !hasValue(property_name); };
  const auto new_end = std::remove_if(sensitive_properties.begin(), sensitive_properties.end(), not_found);
  sensitive_properties.erase(new_end, sensitive_properties.end());

  return sensitive_properties;
}

}  // namespace org::apache::nifi::minifi::encrypt_config
