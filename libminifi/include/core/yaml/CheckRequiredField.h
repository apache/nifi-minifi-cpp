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
#pragma once

#include <string>
#include <memory>
#include <vector>

#include "core/logging/LoggerConfiguration.h"
#include "yaml-cpp/yaml.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace yaml {

bool isFieldPresent(const YAML::Node *yamlNode, const std::string &fieldName);
std::string buildErrorMessage(const YAML::Node *yamlNode, const std::vector<std::string>& alternateFieldNames, const std::string &yamlSection = "");

/**
 * This is a helper function for verifying the existence of a required
 * field in a YAML::Node object. If the field is not present, an error
 * message will be logged and an std::invalid_argument exception will be
 * thrown indicating the absence of the required field in the YAML node.
 *
 * @param yamlNode     the YAML node to check
 * @param fieldName    the required field key
 * @param yamlSection  [optional] the top level section of the YAML config
 *                       for the yamlNode. This is used for generating a
 *                       useful error message for troubleshooting.
 * @param errorMessage [optional] the error message string to use if
 *                       the required field is missing. If not provided,
 *                       a default error message will be generated.
 *
 * @throws std::invalid_argument if the required field 'fieldName' is
 *                               not present in 'yamlNode'
 */
void checkRequiredField(
    const YAML::Node *yamlNode, const std::string &fieldName, const std::shared_ptr<logging::Logger>& logger, const std::string &yamlSection = "", std::string errorMessage = "");

template<typename T>
T parseRequiredField(const YAML::Node* yamlNode,
    const std::vector<std::string>& alternateNames, const std::shared_ptr<logging::Logger>& logger,
    const std::string &yamlSection, std::function<T(const YAML::Node&)> parser, std::string errorMessage = "") {
  for (const auto& name : alternateNames) {
    if (yaml::isFieldPresent(yamlNode, name)) {
      return parser((*yamlNode)[name]);
    }
  }
  if (errorMessage.empty()) {
    errorMessage = buildErrorMessage(yamlNode, alternateNames, yamlSection);
  }
  logger->log_error(errorMessage.c_str());
  throw std::invalid_argument(errorMessage);
}

}  // namespace yaml
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
