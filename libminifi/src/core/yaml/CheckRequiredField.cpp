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

bool isFieldPresent(const YAML::Node *yamlNode, const std::string &fieldName) {
  return bool{yamlNode->as<YAML::Node>()[fieldName]};
}

std::string buildErrorMessage(const YAML::Node *yamlNode, const std::vector<std::string>& alternateFieldNames, const std::string &yamlSection) {
  const YAML::Node name_node = yamlNode->as<YAML::Node>()["name"];
  // Build a helpful error message for the user so they can fix the
  // invalid YAML config file, using the component name if present
  auto fieldListString = utils::StringUtils::join(", ", alternateFieldNames);
  auto errMsg =
      name_node ?
          "Unable to parse configuration file for component named '" + name_node.as<std::string>() + "' as none of the possible required fields [" + fieldListString + "] is available" :
          "Unable to parse configuration file as none of the possible required fields [" + fieldListString + "] is available";
  if (!yamlSection.empty()) {
    errMsg += " [in '" + yamlSection + "' section of configuration file]";
  }
  const YAML::Mark mark = yamlNode->Mark();
  if (!mark.is_null()) {
    errMsg += " [line:column, pos at " + std::to_string(mark.line) + ":" + std::to_string(mark.column) + ", " + std::to_string(mark.pos) + "]";
  }
  return errMsg;
}

void checkRequiredField(const YAML::Node *yamlNode, const std::string &fieldName, const std::shared_ptr<logging::Logger>& logger, const std::string &yamlSection, std::string errorMessage) {
  if (!isFieldPresent(yamlNode, fieldName)) {
    if (errorMessage.empty()) {
      errorMessage = buildErrorMessage(yamlNode, std::vector<std::string>{fieldName}, yamlSection);
    }
    logger->log_error(errorMessage.c_str());
    throw std::invalid_argument(errorMessage);
  }
}

}  // namespace yaml
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
