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

bool isFieldPresent(const YAML::Node &yaml_node, std::string_view field_name);
std::string buildErrorMessage(const YAML::Node &yaml_node, const std::vector<std::string> &alternate_field_names, std::string_view yaml_section = "");

/**
 * This is a helper function for verifying the existence of a required
 * field in a YAML::Node object. If the field is not present, an error
 * message will be logged and an std::invalid_argument exception will be
 * thrown indicating the absence of the required field in the YAML node.
 *
 * @param yaml_node     the YAML node to check
 * @param field_name    the required field key
 * @param yaml_section  [optional] the top level section of the YAML config
 *                       for the yaml_node. This is used for generating a
 *                       useful error message for troubleshooting.
 * @param error_message [optional] the error message string to use if
 *                       the required field is missing. If not provided,
 *                       a default error message will be generated.
 *
 * @throws std::invalid_argument if the required field 'field_name' is
 *                               not present in 'yaml_node'
 */
void checkRequiredField(
    const YAML::Node &yaml_node, std::string_view field_name, std::string_view yaml_section = "", std::string_view error_message = "");

std::string getRequiredField(const YAML::Node &yaml_node, const std::vector<std::string> &alternate_names, std::string_view yaml_section, std::string_view error_message = {});

}  // namespace yaml
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
