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

#include "core/flow/CheckRequiredField.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core::flow {

bool isFieldPresent(const Node &node, std::string_view field_name) {
  return bool{node[field_name]};
}

std::string buildErrorMessage(const Node &node, const std::vector<std::string> &alternate_field_names) {
  const Node name_node = node["name"];
  // Build a helpful error message for the user so they can fix the
  // invalid config file, using the component name if present
  auto field_list_string = utils::StringUtils::join(", ", alternate_field_names);
  std::string err_msg =
      name_node ?
          "Unable to parse configuration file for component named '" + name_node.getString().value() + "' as none of the possible required fields [" + field_list_string + "] is available" :
          "Unable to parse configuration file as none of the possible required fields [" + field_list_string + "] is available";

  err_msg += " [in '" + node.getPath() + "' section of configuration file]";

  if (auto cursor = node.getCursor()) {
    err_msg += " [line:column, pos at " + std::to_string(cursor->line) + ":" + std::to_string(cursor->column) + ", " + std::to_string(cursor->pos) + "]";
  }
  return err_msg;
}

void checkRequiredField(const Node &node, const std::vector<std::string>& field_names, std::string_view error_message) {
  if (std::none_of(field_names.begin(), field_names.end(), [&] (auto& field) {return isFieldPresent(node, field);})) {
    if (error_message.empty()) {
      throw std::invalid_argument(buildErrorMessage(node, field_names));
    }
    throw std::invalid_argument(error_message.data());
  }
}

std::string getRequiredField(const Node &node, const std::vector<std::string> &alternate_names, std::string_view error_message) {
  for (const auto& name : alternate_names) {
    if (isFieldPresent(node, name)) {
      return node[name].getString().value();
    }
  }
  if (error_message.empty()) {
    throw std::invalid_argument(buildErrorMessage(node, alternate_names));
  }
  throw std::invalid_argument(error_message.data());
}

}  // namespace org::apache::nifi::minifi::core::flow
