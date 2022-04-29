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

#include <stdexcept>

#include "core/yaml/CheckRequiredField.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace yaml {

bool isFieldPresent(const YAML::Node &yaml_node, std::string_view field_name) {
  return bool{yaml_node.as<YAML::Node>()[field_name.data()]};
}

std::string buildErrorMessage(const YAML::Node &yaml_node, const std::vector<std::string> &alternate_field_names, std::string_view yaml_section) {
  const YAML::Node name_node = yaml_node.as<YAML::Node>()["name"];
  // Build a helpful error message for the user so they can fix the
  // invalid YAML config file, using the component name if present
  auto field_list_string = utils::StringUtils::join(", ", alternate_field_names);
  std::string err_msg =
      name_node ?
          "Unable to parse configuration file for component named '" + name_node.as<std::string>() + "' as none of the possible required fields [" + field_list_string + "] is available" :
          "Unable to parse configuration file as none of the possible required fields [" + field_list_string + "] is available";
  if (!yaml_section.empty()) {
    err_msg += " [in '" + std::string(yaml_section) + "' section of configuration file]";
  }
  const YAML::Mark mark = yaml_node.Mark();
  if (!mark.is_null()) {
    err_msg += " [line:column, pos at " + std::to_string(mark.line) + ":" + std::to_string(mark.column) + ", " + std::to_string(mark.pos) + "]";
  }
  return err_msg;
}

void checkRequiredField(const YAML::Node &yaml_node, std::string_view field_name, std::string_view yaml_section, std::string_view error_message) {
  if (!isFieldPresent(yaml_node, field_name)) {
    if (error_message.empty()) {
      throw std::invalid_argument(buildErrorMessage(yaml_node, std::vector<std::string>{std::string(field_name)}, yaml_section));
    }
    throw std::invalid_argument(error_message.data());
  }
}

std::string getRequiredField(const YAML::Node &yaml_node, const std::vector<std::string> &alternate_names, std::string_view yaml_section, std::string_view error_message) {
  for (const auto& name : alternate_names) {
    if (yaml::isFieldPresent(yaml_node, name)) {
      return yaml_node[name].as<std::string>();
    }
  }
  if (error_message.empty()) {
    throw std::invalid_argument(buildErrorMessage(yaml_node, alternate_names, yaml_section));
  }
  throw std::invalid_argument(error_message.data());
}

}  // namespace yaml
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
